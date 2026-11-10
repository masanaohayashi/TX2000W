#include "../Source/PluginProcessor.h"
#include <cstdlib>
#include <new>
#include <iostream>
#include <chrono>
#include <ctime>
#include <fstream>

static thread_local bool inCallback=false;
static thread_local size_t allocations=0,deallocations=0;
void* operator new(size_t n) {
    if(inCallback) ++allocations;
    if(auto* p=std::malloc(n?n:1)) return p;
    throw std::bad_alloc();
}
void* operator new[](size_t n) {return ::operator new(n);}
void operator delete(void* p) noexcept {if(inCallback && p) ++deallocations;std::free(p);}
void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete(void* p,size_t) noexcept {::operator delete(p);}
void operator delete[](void* p,size_t) noexcept {::operator delete(p);}

static double cpuSeconds() {
    timespec t{};clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);
    return double(t.tv_sec)+double(t.tv_nsec)*1e-9;
}
static std::vector<float> render(SMU2000AudioProcessor& p,bool variable,int channels=2)
{
    constexpr int total=16384;
    std::vector<float> result(size_t(total),0);
    juce::AudioBuffer<float> buffer(channels,2048);
    juce::MidiBuffer midi;midi.ensureSize(4096);
    int cursor=0,step=0;
    const int sizes[]={0,1,7,257,2048,63};
    const auto start=cpuSeconds();
    while(cursor<total) {
        const int count=std::min(total-cursor,variable?sizes[step++%6]:256);
        buffer.setSize(channels,count,false,false,true);
        midi.clear();
        buffer.clear();
        if (p.inputGain->get()>0)
            for(int ch=0;ch<channels;++ch) for(int n=0;n<count;++n)
                buffer.setSample(ch,n,0.1f*std::sin(float(cursor+n)*0.03f));
        const int at[]={0,8000,12000};
        for(int i=0;i<3;++i) if(at[i]>=cursor && at[i]<cursor+count)
            midi.addEvent(i==1?juce::MidiMessage::noteOff(1,60)
                              :juce::MidiMessage::noteOn(1,i==0?60:64,uint8_t(100)),at[i]-cursor);
        if (4096>=cursor && 4096<cursor+count)
            midi.addEvent(juce::MidiMessage::programChange(1,16),4096-cursor);
        if (6000>=cursor && 6000<cursor+count) {
            const uint8_t reverb[]{0x43,0x10,0x4c,0x02,0x01,0x00,0x01,0x01};
            midi.addEvent(juce::MidiMessage::createSysExMessage(reverb,sizeof(reverb)),6000-cursor);
        }
        inCallback=true;
        p.processBlock(buffer,midi);
        inCallback=false;
        for(int n=0;n<count;++n) {
            for(int ch=0;ch<channels;++ch)
                if(!std::isfinite(buffer.getSample(ch,n))) std::abort();
            if(channels>0) result[size_t(cursor+n)]=buffer.getSample(0,n);
        }
        cursor+=count;
    }
    std::cout<<"callback-thread CPU seconds="<<cpuSeconds()-start
             <<" allocation="<<allocations<<" free="<<deallocations<<std::endl;
    return result;
}
// Sustained organ level over the last half second: the rate converter must not change it.
static double sustainLevel(SMU2000AudioProcessor& p,double rate)
{
    p.prepareToPlay(rate,256);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::programChange(1,16),0);
    midi.addEvent(juce::MidiMessage::noteOn(1,60,uint8_t(100)),1);
    juce::AudioBuffer<float> buffer(2,256);
    const int total=int(rate*1.5),from=int(rate);
    double sum=0;int counted=0;
    for(int at=0;at<total;at+=256) {
        buffer.clear();p.processBlock(buffer,midi);midi.clear();
        if(at>=from) for(int i=0;i<256;++i,++counted) sum+=double(buffer.getSample(0,i))*buffer.getSample(0,i);
    }
    return 10*std::log10(sum/counted);
}
// Held note's pitch, in cents from the unbent note, after `setRange` and a full upward bend.
static double fullBendCents(SMU2000AudioProcessor& p,const juce::MidiBuffer& setRange)
{
    const double rate=44100;
    p.prepareToPlay(rate,256);
    juce::AudioBuffer<float> buffer(2,256);
    // Let earlier notes die and the wheel return, so each measurement starts alone and unbent.
    const auto settle=[&] {
        juce::MidiBuffer off;
        off.addEvent(juce::MidiMessage::allNotesOff(1),0);
        off.addEvent(juce::MidiMessage::pitchWheel(1,8192),1);
        for(int at=0;at<int(rate);at+=256) {buffer.clear();p.processBlock(buffer,off);off.clear();}
    };
    settle();
    const auto period=[&](const juce::MidiBuffer& first) {
        juce::MidiBuffer midi(first);
        std::vector<float> mono;
        for(int at=0;at<int(rate*0.5);at+=256) {
            buffer.clear();p.processBlock(buffer,midi);midi.clear();
            if(at>=int(rate*0.3)) mono.insert(mono.end(),buffer.getReadPointer(0),buffer.getReadPointer(0)+256);
        }
        // Past the lobe around lag 0, the first autocorrelation peak near the highest one is the period.
        const int window=3000,longest=int(rate/30);   // longer lags find the beat between detuned elements
        std::vector<double> c(size_t(longest+1),0.0);
        for(int lag=0;lag<=longest;++lag)
            for(int i=0;i<window;++i) c[size_t(lag)]+=double(mono[size_t(i)])*mono[size_t(i+lag)];
        int pastLobe=1;
        while(pastLobe<longest && c[size_t(pastLobe)]>0) ++pastLobe;
        const double top=*std::max_element(c.begin()+pastLobe,c.end());
        for(int lag=pastLobe+1;lag<longest;++lag) {
            const double a=c[size_t(lag-1)],b=c[size_t(lag)],d=c[size_t(lag+1)];
            if(b>=0.85*top && b>=a && b>=d) return lag+(a-d)/(2*(a-2*b+d));
        }
        return 0.0;
    };
    juce::MidiBuffer start;
    start.addEvent(juce::MidiMessage::programChange(1,80),0);
    start.addEvent(juce::MidiMessage::noteOn(1,60,uint8_t(100)),1);
    const double unbent=period(start);
    juce::MidiBuffer bend(setRange);
    bend.addEvent(juce::MidiMessage::pitchWheel(1,16383),255);
    const double bent=period(bend);
    settle();
    return unbent>0 && bent>0 ? 1200*std::log2(unbent/bent) : 1.0e9;
}
static juce::MidiBuffer rpnBendRange(int semitones)
{
    juce::MidiBuffer m;
    m.addEvent(juce::MidiMessage::controllerEvent(1,101,0),0);
    m.addEvent(juce::MidiMessage::controllerEvent(1,100,0),1);
    m.addEvent(juce::MidiMessage::controllerEvent(1,6,semitones),2);
    return m;
}
static juce::MidiBuffer sysexBendRange(int value)
{
    const uint8_t data[]={0x43,0x10,0x4c,0x08,0x00,0x23,uint8_t(value)};
    juce::MidiBuffer m;
    m.addEvent(juce::MidiMessage::createSysExMessage(data,int(sizeof(data))),0);
    return m;
}
static void addXg(juce::MidiBuffer& m,std::initializer_list<uint8_t> address,uint8_t value,int at=0)
{
    std::vector<uint8_t> data{0x43,0x10,0x4c};
    data.insert(data.end(),address);
    data.push_back(value);
    m.addEvent(juce::MidiMessage::createSysExMessage(data.data(),int(data.size())),at);
}
static void addControl(juce::MidiBuffer& m,int control,int value,int at=0)
{
    m.addEvent(juce::MidiMessage::controllerEvent(1,control,value),at);
}
// A held note after a reset. Returns what it sounded like after `during` was sent while it was held.
static std::vector<float> heldNote(SMU2000AudioProcessor& p,const juce::MidiBuffer& before,const juce::MidiBuffer& during)
{
    p.prepareToPlay(44100,256);
    juce::AudioBuffer<float> buffer(2,256);
    std::vector<float> mono;
    const auto run=[&](juce::MidiBuffer m,double seconds,bool keep) {
        for(int at=0;at<int(44100*seconds);at+=256) {
            buffer.clear();p.processBlock(buffer,m);m.clear();
            if(keep) mono.insert(mono.end(),buffer.getReadPointer(0),buffer.getReadPointer(0)+256);
        }
    };
    juce::MidiBuffer reset;
    addXg(reset,{0x00,0x00,0x7e},0);
    run(reset,1.0,false);
    juce::MidiBuffer program;
    program.addEvent(juce::MidiMessage::programChange(1,48),0);
    run(program,0.1,false);
    run(before,0.1,false);
    juce::MidiBuffer note;
    note.addEvent(juce::MidiMessage::noteOn(1,60,uint8_t(100)),0);
    run(note,0.6,false);
    run(during,0.1,false);
    run({},0.35,true);
    run(reset,1.0,false);
    return mono;
}
static double levelDb(const std::vector<float>& sound)
{
    double sum=0;
    for(const float v:sound) sum+=double(v)*v;
    return 10*std::log10(sum/double(sound.size())+1e-30);
}
// The difference between two sounds, in dB below the first.
static double differenceDb(const std::vector<float>& a,const std::vector<float>& b)
{
    double energy=0,difference=0;
    for(size_t i=0;i<a.size();++i) {
        energy+=double(a[i])*a[i];
        difference+=(double(a[i])-b[i])*(double(a[i])-b[i]);
    }
    return 10*std::log10((difference+1e-30)/(energy+1e-30));
}
static void addGs(juce::MidiBuffer& m,std::initializer_list<uint8_t> addressAndData,int at=0)
{
    std::vector<uint8_t> data{0x41,0x10,0x42,0x12};
    int sum=0;
    for(const uint8_t v:addressAndData) {data.push_back(v);sum+=v;}
    data.push_back(uint8_t((128-sum%128)%128));
    m.addEvent(juce::MidiMessage::createSysExMessage(data.data(),int(data.size())),at);
}
// One drum hit after a reset; `setup` runs before it.
static std::vector<float> drumHit(SMU2000AudioProcessor& p,const juce::MidiBuffer& setup,int key)
{
    p.prepareToPlay(44100,256);
    juce::AudioBuffer<float> buffer(2,256);
    std::vector<float> mono;
    const auto run=[&](juce::MidiBuffer m,double seconds,bool keep) {
        for(int at=0;at<int(44100*seconds);at+=256) {
            buffer.clear();p.processBlock(buffer,m);m.clear();
            if(keep) mono.insert(mono.end(),buffer.getReadPointer(0),buffer.getReadPointer(0)+256);
        }
    };
    run(setup,1.0,false);
    juce::MidiBuffer hit;
    hit.addEvent(juce::MidiMessage::noteOn(10,key,uint8_t(100)),0);
    run(hit,0.4,true);
    return mono;
}
static bool gsMessagesWork(SMU2000AudioProcessor& p)
{
    juce::MidiBuffer gs;
    addGs(gs,{0x40,0x00,0x7f,0x00});
    gs.addEvent(juce::MidiMessage::programChange(1,48),1);
    const auto plain=heldNote(p,gs,{});
    const auto plainXg=heldNote(p,{},{});
    const double base=levelDb(plain);
    const auto check=[&](const char* name,const juce::MidiBuffer& before,const juce::MidiBuffer& during,double expected) {
        const double drop=levelDb(heldNote(p,before,during))-base;
        std::cout<<name<<": "<<drop<<" dB, expected "<<expected<<std::endl;
        return std::abs(drop-expected)<0.5;
    };
    std::cout<<"GS against XG: "<<differenceDb(plain,plainXg)<<" dB"<<std::endl;
    if(differenceDb(plain,plainXg)<-20) return false;              // the reset changes the voice map and the reverb
    juce::MidiBuffer m;
    addGs(m,{0x40,0x11,0x19,0x20});
    if(!check("GS part volume",gs,m,-14.7)) return false;
    m.clear();addGs(m,{0x40,0x00,0x04,0x20});
    if(!check("GS master volume",gs,m,-16.1)) return false;
    m.clear();
    {
        const uint8_t wrongSum[]={0x41,0x10,0x42,0x12,0x40,0x11,0x19,0x20,0x55};
        m.addEvent(juce::MidiMessage::createSysExMessage(wrongSum,int(sizeof(wrongSum))),0);
    }
    if(!check("the checksum is not looked at",gs,m,-14.7)) return false;
    juce::MidiBuffer scMode;
    addGs(scMode,{0x00,0x00,0x7f,0x00});
    scMode.addEvent(juce::MidiMessage::programChange(1,48),1);
    // Each reset moves the 10 ms grid, so compare levels rather than waveforms.
    const double scLevel=levelDb(heldNote(p,scMode,{}));
    std::cout<<"SC-88 mode set against GS reset: "<<scLevel-base<<" dB"<<std::endl;
    if(std::abs(scLevel-base)>0.2) return false;
    // Other GS messages only work in GS mode.
    m.clear();addGs(m,{0x40,0x11,0x19,0x20});
    const double inXg=levelDb(heldNote(p,{},m))-levelDb(plainXg);
    std::cout<<"GS part volume in XG mode: "<<inXg<<" dB"<<std::endl;
    if(std::abs(inXg)>=0.1) return false;
    // Drums: the stick has its own pitch in GS mode, and part controllers reach drum notes.
    constexpr int STICK=31;
    juce::MidiBuffer xgReset,gsReset;
    addXg(xgReset,{0x00,0x00,0x7e},0);
    addGs(gsReset,{0x40,0x00,0x7f,0x00});
    const auto stickXg=drumHit(p,xgReset,STICK),stickGs=drumHit(p,gsReset,STICK);
    std::cout<<"stick, GS against XG: "<<differenceDb(stickXg,stickGs)<<" dB"<<std::endl;
    if(differenceDb(stickXg,stickGs)<-3) return false;
    juce::MidiBuffer darker(xgReset);
    darker.addEvent(juce::MidiMessage::controllerEvent(10,74,0),1);
    const double level=levelDb(drumHit(p,darker,38))-levelDb(drumHit(p,xgReset,38));
    std::cout<<"drum with CC74 = 0: "<<level<<" dB"<<std::endl;
    return std::abs(level)>0.5;
}
static bool firmwareRulesHold(SMU2000AudioProcessor& p)
{
    // A slide from a high key takes the level of the wave chosen by that key.
    juce::MidiBuffer organ,organSlide;
    organ.addEvent(juce::MidiMessage::programChange(1,19),0);
    organSlide=organ;
    addControl(organSlide,84,112,1);
    const double slideDrop=levelDb(heldNote(p,organSlide,{}))-levelDb(heldNote(p,organ,{}));
    std::cout<<"CC84 from key 112: "<<slideDrop<<" dB, expected -4.4"<<std::endl;
    if(std::abs(slideDrop+4.4)>0.5) return false;
    // A faster attack on a held note also speeds up the filter envelope.
    juce::MidiBuffer slowAttack,fastAttack;
    addControl(slowAttack,73,112);
    addControl(fastAttack,73,16);
    const double opened=levelDb(heldNote(p,slowAttack,fastAttack))-levelDb(heldNote(p,slowAttack,{}));
    std::cout<<"CC73 on a held note: "<<opened<<" dB"<<std::endl;
    if(opened<6) return false;
    juce::MidiBuffer silentKit;
    addXg(silentKit,{0x00,0x00,0x7e},0);
    silentKit.addEvent(juce::MidiMessage::controllerEvent(10,0,126),1);
    silentKit.addEvent(juce::MidiMessage::programChange(10,2),2);
    const double silence=levelDb(drumHit(p,silentKit,38));
    std::cout<<"silent kit: "<<silence<<" dBFS"<<std::endl;
    return silence<-90;
}
static bool xgMessagesWork(SMU2000AudioProcessor& p)
{
    const auto plain=heldNote(p,{},{});
    const double base=levelDb(plain);
    const auto check=[&](const char* name,const juce::MidiBuffer& before,const juce::MidiBuffer& during,double expected) {
        const double drop=levelDb(heldNote(p,before,during))-base;
        std::cout<<name<<": "<<drop<<" dB, expected "<<expected<<std::endl;
        return std::abs(drop-expected)<0.5;
    };
    const auto silent=[&](const char* name,const juce::MidiBuffer& before,const juce::MidiBuffer& during) {
        const double level=levelDb(heldNote(p,before,during));
        std::cout<<name<<": "<<level<<" dBFS"<<std::endl;
        return level<-40;
    };
    juce::MidiBuffer m;
    addXg(m,{0x08,0x00,0x0b},0x20);
    if(!check("part volume on a held note",{},m,-13.5)) return false;
    m.clear();addXg(m,{0x00,0x00,0x04},0x20);
    if(!check("master volume on a held note",{},m,-15.0)) return false;
    m.clear();addXg(m,{0x00,0x00,0x05},0x20);
    if(!check("master attenuator",{},m,-5.0)) return false;
    m.clear();
    {
        const uint8_t universal[]={0x7f,0x7f,0x04,0x01,0x00,0x20};
        m.addEvent(juce::MidiMessage::createSysExMessage(universal,int(sizeof(universal))),0);
    }
    if(!check("universal master volume",{},m,-15.0)) return false;
    m.clear();
    {
        // Byte count (data only), address, data, checksum.
        const uint8_t bulk[]={0x43,0x00,0x4c,0x00,0x01,0x08,0x00,0x0b,0x20,0x7f&uint8_t(-(0x01+0x08+0x0b+0x20))};
        m.addEvent(juce::MidiMessage::createSysExMessage(bulk,int(sizeof(bulk))),0);
    }
    if(!check("bulk dump",{},m,-13.5)) return false;
    // Receive switches: the switch is sent before the note, the message while it is held.
    juce::MidiBuffer off,volume;
    addXg(off,{0x08,0x00,0x39},0);
    addControl(volume,7,30);
    if(!check("receive volume off",off,volume,0.0)) return false;
    off.clear();addXg(off,{0x08,0x00,0x33},0);
    if(!check("receive control change off",off,volume,0.0)) return false;
    off.clear();addXg(off,{0x08,0x00,0x35},0);
    if(!silent("receive note off",off,{})) return false;
    off.clear();addXg(off,{0x08,0x00,0x6d},0x7f);
    if(!silent("velocity below the limit",off,{})) return false;
    m.clear();addXg(m,{0x08,0x00,0x05},1);
    if(!silent("mono/poly stops the note",{},m)) return false;
    juce::MidiBuffer hold;
    addControl(hold,64,127);
    m.clear();addControl(m,120,0);
    if(!silent("all sound off under the damper",hold,m)) return false;
    // Notes the note was already holding: resonance moves, a reverb type change does not touch the chorus.
    const auto samePlain=heldNote(p,{},{});
    std::cout<<"same input twice: "<<differenceDb(plain,samePlain)<<" dB"<<std::endl;
    if(differenceDb(plain,samePlain)>-90) return false;
    m.clear();addControl(m,71,127);
    std::cout<<"resonance on a held note: "<<differenceDb(plain,heldNote(p,{},m))<<" dB"<<std::endl;
    if(differenceDb(plain,heldNote(p,{},m))<-40) return false;
    juce::MidiBuffer chorus,chorusAndType;
    addControl(chorus,91,0);addControl(chorus,93,100,1);
    chorusAndType=chorus;
    addXg(chorusAndType,{0x02,0x01,0x00},0x02,2);
    const double kept=differenceDb(heldNote(p,chorus,{}),heldNote(p,chorusAndType,{}));
    std::cout<<"reverb type under a chorus: "<<kept<<" dB"<<std::endl;
    return kept<-60;
}
int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    std::ofstream audio;
    if (const char* path=std::getenv("SMU_TEST_AUDIO_OUTPUT")) audio.open(path,std::ios::binary);
    SMU2000AudioProcessor p;
    for(double rate:{44100.0,48000.0,96000.0}) {
        p.prepareToPlay(rate,128);
        if(!p.engine.isReady()) return 1;
        juce::MemoryBlock baseline;p.getStateInformation(baseline);
        p.setStateInformation(baseline.getData(),int(baseline.getSize()));
        const auto regular=render(p,false);
        if (audio.is_open()) audio.write(reinterpret_cast<const char*>(regular.data()),
                                        std::streamsize(regular.size()*sizeof(float)));
        p.setStateInformation(baseline.getData(),int(baseline.getSize()));
        const auto varying=render(p,true);
        float difference=0,peak=0;
        for(size_t i=0;i<regular.size();++i) {
            difference=std::max(difference,std::abs(regular[i]-varying[i]));
            peak=std::max(peak,std::abs(regular[i]));
        }
        std::cout<<rate<<" Hz: peak="<<peak<<" partition error="<<difference
                 <<" latency="<<p.getLatencySamples()<<std::endl;
        if(peak<0.001f || difference>1e-6f || allocations || deallocations) return 2;
        if(rate==44100 && p.getLatencySamples()!=0) return 3;
    }
    const double level44=sustainLevel(p,44100);
    for(double rate:{48000.0,96000.0,22050.0}) {
        const double level=sustainLevel(p,rate);
        std::cout<<rate<<" Hz sustain "<<level<<" dB, 44.1 kHz "<<level44<<" dB"<<std::endl;
        if(std::abs(level-level44)>0.2) return 13;
    }
    {
        const struct {const char* name;juce::MidiBuffer range;double cents;} bends[]={
            {"default range",juce::MidiBuffer(),200},
            {"RPN 12",rpnBendRange(12),1200},
            {"RPN 24",rpnBendRange(24),2400},
            {"RPN over 24",rpnBendRange(30),2400},
            {"RPN 0",rpnBendRange(0),0},
            {"SysEx -4",sysexBendRange(0x3c),-400},
            {"SysEx -48",sysexBendRange(0x10),-2400},
            {"RPN 2 + data increment x3",[] {auto m=rpnBendRange(2);for(int i=3;i<6;++i) addControl(m,96,0,i);return m;}(),500},
            {"RPN 2 + data decrement",[] {auto m=rpnBendRange(2);addControl(m,97,5,3);return m;}(),100},
        };
        for(const auto& b:bends) {
            const double cents=fullBendCents(p,b.range);
            std::cout<<"bend "<<b.name<<": "<<cents<<" cents, expected "<<b.cents<<std::endl;
            if(std::abs(cents-b.cents)>10) return 16;
        }
    }
    if(!xgMessagesWork(p)) return 17;
    if(!gsMessagesWork(p)) return 18;
    if(!firmwareRulesHold(p)) return 19;
    p.prepareToPlay(96000,128);
    render(p,true,1);render(p,true,0);
    {
        juce::TemporaryFile fixture(".mid");
        const uint8_t bytes[]={'M','T','h','d',0,0,0,6,0,0,0,1,0,96,
            'M','T','r','k',0,0,0,12,0,0x90,60,100,96,0x80,60,0,0,0xff,0x2f,0};
        if(!fixture.getFile().replaceWithData(bytes,sizeof(bytes))) return 15;
        const auto sequenceFile=argc>1?juce::File(argv[1]):fixture.getFile();
        juce::String error;
        if(!p.loadSequenceFile(sequenceFile,error)) return 4;
        p.engine.sequence().play();
        render(p,true);
        // Replacement retires the previous file on the loader, not the callback.
        if(!p.loadSequenceFile(sequenceFile,error)) return 5;
        p.engine.sequence().play();render(p,true);
    }
    // Panel buttons act in the callback: change the variation type there, too.
    p.engine.button(3,true);render(p,true);p.engine.button(3,false);
    p.engine.button(11,true);render(p,true);p.engine.button(11,false);
    p.engine.button(11,true);render(p,true);p.engine.button(11,false);
    for(int i=0;i<4;++i) {p.engine.turn(7);render(p,true);}
    p.inputGain->setValueNotifyingHost(0.5f);
    render(p,true);
    p.inputGain->setValueNotifyingHost(0);
    if(allocations || deallocations) return 6;
    // Overload must fail silent, then recover during non-RT prepare.
    juce::MidiBuffer overflow;
    for(int i=0;i<5000;++i) overflow.addEvent(juce::MidiMessage::noteOn(1,60,uint8_t(100)),0);
    juce::AudioBuffer<float> block(2,128);
    inCallback=true;p.processBlock(block,overflow);inCallback=false;
    std::cout<<"overflow: ready="<<p.engine.isReady()<<" peak="<<block.getMagnitude(0,128)
             <<" allocations="<<allocations<<" frees="<<deallocations<<std::endl;
    if(p.engine.isReady() || block.getMagnitude(0,128)!=0 || allocations || deallocations) return 10;
    p.prepareToPlay(44100,128);
    if(!p.engine.isReady()) return 11;
    std::cout<<"PASS: direct callback rendering, no C++ heap operations, variable/zero/oversized blocks, mono/stereo/no outputs, MIDI file replacement"<<std::endl;
}
