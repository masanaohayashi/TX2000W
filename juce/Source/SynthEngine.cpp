#include "SynthEngine.h"
#include "compat/platform.h"
#include "native/panel.h"
#include "roms_dir.h"
#include "MinimumPhaseSRC.hpp"
#include <thread>
#include <mutex>
#include <chrono>

#if JUCE_MAC || JUCE_IOS
juce::File appGroupRomDirectory(); // AppGroupRoms.mm
#endif
#if JUCE_IOS
bool copyRomFolder (juce::URL folder, const juce::File& destination, juce::String& error); // AppGroupRoms.mm
#endif

namespace {
using smu2000::native_engine;
using smu2000::native_panel;

#if JUCE_IOS
juce::File importedRomDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("roms");
}
#endif

juce::File romDirectory()
{
    // Match the upstream ROM override; also lets headless tests avoid app containers.
    const auto overridePath = juce::SystemStats::getEnvironmentVariable("S_MU2000_ROMS", {});
    if (overridePath.isNotEmpty()) return juce::File(overridePath);
   #if JUCE_IOS
    if (importedRomDirectory().getChildFile ("mu2000_flash.bin").existsAsFile()) return importedRomDirectory();
   #endif
   #if JUCE_MAC || JUCE_IOS
    const auto appGroup = appGroupRomDirectory();
    if (appGroup.getChildFile ("mu2000_flash.bin").existsAsFile()) return appGroup;
    auto shared = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
        .getChildFile ("Library/Group Containers/group.tokyo.studio-r.synths/TX2000W/roms");
    if (shared.getChildFile ("mu2000_flash.bin").existsAsFile()) return shared;
   #else
    // Windows: %APPDATA%/STUDIO-R/TX2000W/roms
    const auto configured = juce::File (juce::String::fromUTF8 (smu2000::config_dir().c_str())).getChildFile ("roms");
    if (configured.getChildFile ("mu2000_flash.bin").existsAsFile()) return configured;
   #endif
    // Development fallback. No proprietary ROM is embedded in the plug-in.
    return juce::File (__FILE__).getParentDirectory().getParentDirectory()
        .getParentDirectory().getChildFile ("roms");
}

bool readFile (const juce::File& file, std::vector<u8>& out)
{
    juce::MemoryBlock data;
    if (!file.loadFileAsData (data)) return false;
    const auto* bytes = static_cast<const u8*> (data.getData());
    out.assign (bytes, bytes + data.getSize());
    return true;
}

static_assert(std::is_trivially_copyable_v<native_panel::view>);
static_assert(std::atomic<bool>::is_always_lock_free);
static_assert(std::atomic<unsigned>::is_always_lock_free);
static_assert(std::atomic<uint64_t>::is_always_lock_free);

constexpr int NATIVE_RATE = 44100;
constexpr int MAX_CHUNK = 1024;      // Longest render between MIDI dispatches.
}

struct SynthEngine::Impl
{
    // Non-RT callers serialize, close entry, and wait for the current callback.
    // process() makes ONE ownership attempt, never waits or takes this mutex.
    std::mutex maintenanceMutex;
    juce::String missingRom; // 読み込みのときだけ書く
    std::atomic<bool> maintenance{false},ready{false};
    std::atomic_flag inUse=ATOMIC_FLAG_INIT;
    struct Maintenance {
        Impl& owner;
        std::unique_lock<std::mutex> lock;
        explicit Maintenance(Impl& p):owner(p),lock(p.maintenanceMutex) {
            owner.maintenance.store(true,std::memory_order_release);
            while(owner.inUse.test_and_set(std::memory_order_acquire))
                std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        ~Maintenance() {
            owner.inUse.clear(std::memory_order_release);
            owner.maintenance.store(false,std::memory_order_release);
        }
    };
    std::atomic<int> phase{0};
    std::unique_ptr<native_engine> machine;
    std::unique_ptr<::xg::voice_rom> voices;
    SequencePlayer sequence;
    juce::MemoryBlock pending;
    double rate=NATIVE_RATE;
    int latency=0;
    std::atomic<uint64_t> buttons{0};
    std::atomic<int> encoder{0};
    native_panel panel;
    uint64_t oldButtons=0;
    std::array<native_panel::view,8> views{};
    std::atomic<unsigned> viewWrite{0},viewRead{0};
    std::vector<u8> font;
    juce::SmoothedValue<float> inputLevel;
    // Scratch for one chunk: no allocation on the audio thread.
    std::array<float,MAX_CHUNK> adLeft{},adRight{},renderLeft{},renderRight{};
    // Conversion to other host rates: the minimum-phase polyphase FIR shared with SQR
    // (MinimumPhaseSRC.hpp). prepare() builds the kernel and reserves the history, so the
    // audio thread never allocates. Minimum phase, so no latency to report.
    patchsrc::MinimumPhaseStream src;
    std::array<float,MAX_CHUNK> nativeInputLeft{},nativeInputRight{};
    float previousInput[2]{};

    bool convertsRate() const { return std::abs(rate-NATIVE_RATE) >= 0.001; }

    // Render host samples [from, from+n) (n <= MAX_CHUNK). ad* are the host input, already scaled.
    void renderChunk(int n, const float* adL, const float* adR, float* left, float* right)
    {
        if (!convertsRate()) { machine->render(left,right,n,adL,adR); return; }
        const double perNative=rate/NATIVE_RATE;
        // Host frames per pass, so one pass never needs more than MAX_CHUNK native frames.
        const int pass=std::max(1,int(double(MAX_CHUNK-3)/src.step));
        int done=0;
        while (done<n)
        {
            // Render only the native frames this pass reads (the last output's, +1), in one call.
            const int stretch=std::min(n-done,pass);
            const auto need=size_t(src.pos+double(stretch-1)*src.step)+2;
            const int needed=need>src.frames()?int(std::min<size_t>(need-src.frames(),MAX_CHUNK)):0;
            if (needed>0)
            {
                // Input at the native sample times, between the host samples around them.
                for (int i=0;i<needed;++i)
                {
                    const double host=double(done)+(double(src.frames()+size_t(i))-src.pos)*perNative;
                    const auto at=[&](const float* p,float before,double x) {
                        if (x<=-1) return before;
                        const int i0=int(std::floor(x));
                        const float a=i0<0?before:p[std::min(i0,n-1)];
                        const float b=p[std::clamp(i0+1,0,n-1)];
                        return a+float(x-double(i0))*(b-a);
                    };
                    nativeInputLeft[size_t(i)]=at(adL,previousInput[0],host);
                    nativeInputRight[size_t(i)]=at(adR,previousInput[1],host);
                }
                machine->render(renderLeft.data(),renderRight.data(),needed,nativeInputLeft.data(),nativeInputRight.data());
                for (int i=0;i<needed;++i) src.push(renderLeft[size_t(i)],renderRight[size_t(i)]);
            }
            for (int s=0;s<stretch;++s)
            {
                left[done+s]=right[done+s]=0;
                src.next(left[done+s],right[done+s]);
            }
            src.trim();
            done+=stretch;
        }
        previousInput[0]=adL[n-1];
        previousInput[1]=adR[n-1];
    }

    void publishView() {
        const auto w=viewWrite.load(std::memory_order_relaxed);
        if(w-viewRead.load(std::memory_order_acquire)==views.size()) return;
        views[w%views.size()]=panel.capture(*machine);
        viewWrite.store(w+1,std::memory_order_release);
    }
    void resetHistory() {
        src.buf.assign(src.hist*2,0.f);src.pos=double(src.hist); // silence history, within the reserve
        previousInput[0]=previousInput[1]=0;
        inputLevel.reset(rate,0.02);inputLevel.setCurrentAndTargetValue(0);
        oldButtons=0;buttons.store(0);encoder.store(0);
    }
    bool initialise() {
        const auto dir=romDirectory();
        std::vector<u8> prog,wave[4];
        // 足りないものを言えるように、読めなかった最初のファイルを覚える
        const auto need=[&](const char* name,std::vector<u8>& out) {
            if(readFile(dir.getChildFile(name),out)) return true;
            if(missingRom.isEmpty()) missingRom=name;
            return false;
        };
        missingRom.clear();
        bool ok=need("mu2000_flash.bin",prog);
        // 文字の ROM は standin に置いてあることもある
        ok=ok && (readFile(dir.getChildFile("standin/hd44780u_b04.bin"),font) || need("hd44780u_b04.bin",font));
        for(int i=0;i<4 && ok;++i) ok=need((juce::String("dump/")+smu2000::kWaveRomNames[i]).toRawUTF8(),wave[i]);
        if(!ok) {phase.store(3);return false;}
        const std::vector<u8>* const parts[4]={&wave[0],&wave[1],&wave[2],&wave[3]};
        ::xg::set_fx_type_rom(prog);
        machine=std::make_unique<native_engine>();
        if(!machine->load(std::move(prog),parts)) {phase.store(3);machine.reset();return false;}
        voices=std::make_unique<::xg::voice_rom>(machine->program_rom_shared());
        phase.store(1);
        return true;
    }
    void applyPending() {
        const auto* data=static_cast<const u8*>(pending.getData());
        const auto size=pending.getSize();
        // パネルの設定を足す前の保存は音源の状態だけ
        if(size>native_panel::SAVED_SIZE && machine->load_state(data,size-native_panel::SAVED_SIZE))
            panel.load(*machine,data+size-native_panel::SAVED_SIZE);
        else
            machine->load_state(data,size);
        pending.reset();
    }
};
SynthEngine::SynthEngine():impl(std::make_unique<Impl>()) {}
SynthEngine::~SynthEngine()=default;
void SynthEngine::prepare(double rate,int)
{
    Impl::Maintenance access(*impl);
    impl->ready.store(false);
    impl->rate=std::isfinite(rate) && rate>=8000 && rate<=384000 ? rate:NATIVE_RATE;
    impl->latency=0; // minimum phase: nothing for the host to compensate
    impl->src.prepare(NATIVE_RATE,impl->rate,MAX_CHUNK+8); // builds the kernel (tens of ms)
    if(!impl->machine && !impl->initialise()) return;
    if(impl->pending.getSize()!=0) impl->applyPending();
    impl->machine->discard_sound();
    impl->machine->clear_midi_overflow();
    impl->sequence.stop();
    impl->sequence.processStamped(0,impl->rate,[](const uint8_t*,int,int,int){},[]{});
    impl->resetHistory();
    impl->phase.store(2);impl->ready.store(true,std::memory_order_release);
    impl->publishView();
}
#if JUCE_IOS
bool SynthEngine::copyRomFolder(const juce::URL& folder, juce::String& error)
{
    // 全部揃ってから入れ替える。途中で失敗しても、いま動いている ROM を壊さない
    const auto target=importedRomDirectory();
    const auto incoming=target.getSiblingFile("roms-incoming");
    incoming.deleteRecursively();
    if(!::copyRomFolder(folder,incoming,error)) {
        incoming.deleteRecursively();
        return false;
    }
    target.deleteRecursively();
    if(incoming.moveFileTo(target)) return true;
    error="Cannot store the ROMs";
    return false;
}
bool SynthEngine::reloadRoms(juce::String& error)
{
    {
        Impl::Maintenance access(*impl);
        impl->ready.store(false);
        impl->machine.reset();
    }
    prepare(impl->rate,0);
    if(isReady()) return true;
    error=status();
    return false;
}
#endif
bool SynthEngine::isReady() const {return impl->ready.load(std::memory_order_acquire);}
int SynthEngine::latencySamples() const {return impl->latency;}
SequencePlayer& SynthEngine::sequence() {return impl->sequence;}
juce::String SynthEngine::status() const
{
    switch(impl->phase.load()) {
        case 0:return "Loading ROMs...";
        case 1:return "Preparing MU2000...";
        case 2:return "MU2000 / MIDI IN A";
       #if JUCE_IOS
        case 3:return "ROM missing: "+impl->missingRom+" (open the ROM folder)";
       #else
        case 3:return "ROM missing: "+impl->missingRom;
       #endif
        case 6:return "MIDI queue overflow; reprepare required";
        default:return "MU2000 error";
    }
}
void SynthEngine::process(juce::AudioBuffer<float>& out,const juce::MidiBuffer& input, int inputChannels, float inputGain)
{
    if(out.getNumSamples()==0 || impl->maintenance.load(std::memory_order_acquire)
       || impl->inUse.test_and_set(std::memory_order_acquire)) { out.clear(); return; }
    struct Release {std::atomic_flag& flag;~Release(){flag.clear(std::memory_order_release);}} release{impl->inUse};
    if(!isReady()) { out.clear(); return; }
    const smu2000::denormals_off noDenormals;
    auto& machine=*impl->machine;
    const auto now=impl->buttons.load(std::memory_order_relaxed);
    const auto changed=now^impl->oldButtons;
    for(int b=0;b<64;++b)
        if(changed>>b&1) {
            if(now>>b&1) impl->panel.press(machine,b);
            else impl->panel.release(machine,b);
        }
    impl->oldButtons=now;
    impl->panel.tick(machine,out.getNumSamples());
    if(const auto delta=impl->encoder.exchange(0)) impl->panel.turn(machine,delta);
    inputChannels=juce::jlimit(0,out.getNumChannels(),inputChannels);
    impl->inputLevel.setTargetValue(juce::jlimit(0.0f,1.0f,inputGain));
    auto event=input.begin();
    int rendered=0;
    const int total=out.getNumSamples();
    auto dispatchHost=[&](int at) {
        while(event!=input.end() && (*event).samplePosition<=at) {
            const auto m=*event;
            for(int i=0;i<m.numBytes;++i) machine.midi(m.data[i],0);
            ++event;
        }
    };
    // The buffer holds the host's input until it is overwritten with the output.
    auto renderUntil=[&](int until) {
        while(rendered<until) {
            dispatchHost(rendered);
            int n=std::min(until-rendered,MAX_CHUNK);
            if(event!=input.end()) n=std::min(n,std::max(1,(*event).samplePosition-rendered));
            for(int i=0;i<n;++i) {
                const float level=impl->inputLevel.getNextValue();
                const auto read=[&](int ch) {
                    const float value=inputChannels>ch && level>0 ? out.getSample(ch,rendered+i):0;
                    return std::isfinite(value) ? value*level : 0.0f;
                };
                impl->adLeft[size_t(i)]=read(0);
                impl->adRight[size_t(i)]=inputChannels>1 ? read(1) : impl->adLeft[size_t(i)];
            }
            float left[MAX_CHUNK],right[MAX_CHUNK];
            impl->renderChunk(n,impl->adLeft.data(),impl->adRight.data(),left,right);
            if(out.getNumChannels()==1)
                for(int i=0;i<n;++i) out.setSample(0,rendered+i,(left[i]+right[i])*0.5f);
            else if(out.getNumChannels()>=2) {
                out.copyFrom(0,rendered,left,n);
                out.copyFrom(1,rendered,right,n);
                for(int ch=2;ch<out.getNumChannels();++ch) out.clear(ch,rendered,n);
            }
            rendered+=n;
        }
    };
    bool transportApplied=false;
    impl->sequence.processStamped(total,impl->rate,
        [&](const uint8_t* bytes,int size,int offset,int port) {
            renderUntil(offset);
            if(transportApplied) dispatchHost(offset);
            for(int i=0;i<size;++i) machine.midi(bytes[i],port);
        },
        [&] {transportApplied=true;dispatchHost(0);});
    renderUntil(total);
    if(machine.midi_overflowed()) {
        out.clear();impl->ready.store(false);
        impl->phase.store(6);
    }
    impl->publishView();
}
bool SynthEngine::readPanel(PanelSnapshot& dest)
{
    JUCE_ASSERT_MESSAGE_THREAD
    const auto r=impl->viewRead.load(std::memory_order_relaxed);
    const auto w=impl->viewWrite.load(std::memory_order_acquire);
    if(r==w) return false;
    // Copy before releasing the slot; the producer may reuse it immediately.
    const auto view=impl->views[(w-1)%impl->views.size()];
    impl->viewRead.store(w,std::memory_order_release);
    native_panel::lcd_frame frame;
    native_panel::compose(view,*impl->voices,frame);
    constexpr size_t GLYPH_ROWS=8, GLYPH_STRIDE=16, CUSTOM_CHARACTERS=16;
    for(int row=0;row<native_panel::ROWS;++row)
        for(int col=0;col<native_panel::LINE_LENGTH;++col) {
            const size_t code=frame.text[row][col];
            const size_t at=size_t(row*native_panel::LINE_LENGTH+col)*GLYPH_ROWS;
            for(size_t y=0;y<GLYPH_ROWS;++y) {
                if(code<CUSTOM_CHARACTERS) dest.dots[at+y]=frame.cgram[(code&7)*GLYPH_ROWS+y];
                else {
                    const size_t glyph=code*GLYPH_STRIDE+y;
                    dest.dots[at+y]=glyph<impl->font.size()?impl->font[glyph]:0;
                }
            }
        }
    dest.leds=view.leds();
    dest.contrast=view.contrast+1;
    dest.lcdOn=true;
    return true;
}
void SynthEngine::button(int index,bool down)
{
    if (index<0 || index>=64) return;
    const auto bit=uint64_t(1)<<index;
    if (down) impl->buttons.fetch_or(bit); else impl->buttons.fetch_and(~bit);
}
void SynthEngine::turn(int delta) { impl->encoder.fetch_add(delta); }
void SynthEngine::takeOutgoing(juce::MidiBuffer& out)
{
    if(!isReady()) return;
    u8 message[native_panel::DUMP_MESSAGE_LIMIT];
    while(const int size=impl->panel.next_dump_message(*impl->machine,message))
        out.addEvent(message,size,0);
}

juce::MemoryBlock SynthEngine::save()
{
    Impl::Maintenance access(*impl);
    if(!impl->machine || !isReady()) return impl->pending;
    auto bytes=impl->machine->save_state();
    const auto instrument=bytes.size();
    bytes.resize(instrument+native_panel::SAVED_SIZE);
    impl->panel.save(*impl->machine,bytes.data()+instrument);
    return {bytes.data(),bytes.size()};
}
void SynthEngine::restore(const void* data,size_t size)
{
    if(!data || size==0 || size>64*1024*1024) return;
    Impl::Maintenance access(*impl);
    impl->pending.replaceAll(data,size);
    if(!impl->machine) return;
    impl->ready.store(false);
    impl->applyPending();
    impl->machine->discard_sound();
    impl->sequence.stop();
    impl->sequence.processStamped(0,impl->rate,[](const uint8_t*,int,int,int){},[]{});
    impl->resetHistory();
    impl->phase.store(2);impl->ready.store(true,std::memory_order_release);
    impl->publishView();
}
