#include "../Source/PluginProcessor.h"
#include <iostream>
#include <cmath>

static float render(SMU2000AudioProcessor& p,int samples,juce::MidiBuffer midi={})
{
    float peak=0;
    while(samples>0) {
        const int n=std::min(samples,256);
        juce::AudioBuffer<float> block(2,n);
        block.clear();
        p.processBlock(block,midi);midi.clear();
        for(int ch=0;ch<2;++ch) for(int i=0;i<n;++i) {
            const auto v=block.getSample(ch,i);
            if(!std::isfinite(v)) std::abort();
            peak=std::max(peak,std::abs(v));
        }
        samples-=n;
    }
    return peak;
}
int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    // Legacy v1 sessions have only outputGain before the core state.
    {
        SMU2000AudioProcessor legacy;
        juce::MemoryBlock bytes;
        juce::MemoryOutputStream stream(bytes,false);
        stream.writeInt(0x4d553231); stream.writeFloat(0.25f);
        legacy.inputGain->setValueNotifyingHost(0.9f);
        legacy.setStateInformation(bytes.getData(),int(bytes.getSize()));
        if (legacy.inputGain->get()!=0 || legacy.outputGain->get()!=0.25f) return 19;
    }
    juce::MemoryBlock state;
    PanelSnapshot savedPanel;
    bool checkPanel=false;
    if(argc>1) {
        if(!juce::File(argv[1]).loadFileAsData(state)) return 5;
    } else {
        SMU2000AudioProcessor source;
        source.setNonRealtime(true);source.prepareToPlay(44100,256);
        render(source,44100);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::programChange(1,16),0);
        midi.addEvent(juce::MidiMessage::controllerEvent(1,64,127),1);
        midi.addEvent(juce::MidiMessage::noteOn(1,60,uint8_t(100)),2);
        const auto sounding=render(source,22050,midi);
        std::cout<<"source initial peak="<<sounding<<" status="<<source.engine.status()<<std::endl;
        if(sounding<0.001f) return 2;
        source.outputGain->setValueNotifyingHost(0.42f);
        source.inputGain->setValueNotifyingHost(0.31f);
        source.getStateInformation(state);
        source.engine.readPanel(savedPanel); // discard snapshots queued during boot
        render(source,4410);
        checkPanel=source.engine.readPanel(savedPanel);
        if(render(source,4410)<0.001f) return 6; // saving must NOT silence the live instance
        std::cout<<"source peak="<<sounding<<" state bytes="<<state.getSize()<<std::endl;
    }
    SMU2000AudioProcessor restored;
    restored.setNonRealtime(true);
    restored.setStateInformation(state.getData(),int(state.getSize()));
    restored.prepareToPlay(44100,256);
    const auto unwanted=render(restored,44100);
    std::cout<<"restored with NO MIDI: peak="<<unwanted<<" gain="<<restored.outputGain->get()<<std::endl;
    if(unwanted>0.0001f) { std::cerr<<"FAIL: restored a held voice/tail"<<std::endl; return 1; }
    if(std::abs(restored.outputGain->get()-0.42f)>1e-6f) return 3;
    if(!restored.engine.isReady()) return 7;
    if(argc==1 && std::abs(restored.inputGain->get()-0.31f)>1e-6f) return 18;
    PanelSnapshot restoredPanel;
    restored.engine.readPanel(restoredPanel);
    render(restored,4410);
    if(checkPanel && restored.engine.readPanel(restoredPanel))
        // Last cell is the MIDI activity indicator, not part of the patch name.
        for(size_t i=12*8;i<23*8;++i)
            if(savedPanel.dots[i]!=restoredPanel.dots[i]) {
                std::cerr<<"FAIL: saved patch LCD changed at "<<i<<std::endl;return 8;
            }
    juce::MidiBuffer note;note.addEvent(juce::MidiMessage::noteOn(1,64,uint8_t(100)),0);
    const auto fresh=render(restored,22050,note);
    std::cout<<"fresh note peak="<<fresh<<std::endl;
    if(fresh<0.001f) return 4;
    juce::MidiBuffer off;off.addEvent(juce::MidiMessage::noteOff(1,64),0);
    render(restored,44100,off);
    const auto released=render(restored,44100);
    std::cout<<"released without pedal-up: peak="<<released<<std::endl;
    if(released>0.001f) return 9;
    render(restored,22050,note);
    juce::TemporaryFile sequenceFile(".mid");
    const uint8_t smf[]={ 'M','T','h','d',0,0,0,6,0,0,0,1,0,96,
                         'M','T','r','k',0,0,0,12,0,0x90,60,100,96,0x80,60,0,0,0xff,0x2f,0 };
    if(!sequenceFile.getFile().replaceWithData(smf,sizeof(smf))) return 13;
    std::string error;
    if(!restored.engine.sequence().load(sequenceFile.getFile().getFullPathName().toStdString(),error)) return 14;
    restored.engine.sequence().play();
    if(!restored.engine.sequence().isPlaying()) return 15;
    restored.setStateInformation(state.getData(),int(state.getSize()));
    const auto live=render(restored,44100);
    std::cout<<"active-instance restore: peak="<<live<<std::endl;
    if(live!=0 || !restored.engine.isReady()) return 10;
    if(restored.engine.sequence().isPlaying() || restored.engine.sequence().seconds()!=0) return 16;
    render(restored,22050,note);
    restored.prepareToPlay(48000,256);
    if(render(restored,48000)!=0 || !restored.engine.isReady()) return 11;
    if(render(restored,24000,note)<0.001f) return 12;
    std::cout<<"PASS: silent startup/live restore/reprepare; patch and gain retained; save is non-disruptive; fresh notes play and release"<<std::endl;
}
