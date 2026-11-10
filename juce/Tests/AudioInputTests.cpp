#include "../Source/PluginProcessor.h"
#include <iostream>
#include <cmath>
static float render(SMU2000AudioProcessor& p, bool signal, bool initialise)
{
    juce::AudioBuffer<float> buffer(2,257);
    juce::MidiBuffer midi;
    if(initialise) {
        const uint8_t bytes[]={0xf0,0x43,0x10,0x4c,0x10,0,0x0b,127,0xf7};
        midi.addEvent(bytes,sizeof(bytes),0); // A/D1 part volume; the default is zero.
    }
    float peak=0;
    for(int block=0;block<200;++block) {
        for(int i=0;i<257;++i) {
            buffer.setSample(0,i,signal?0.2f*std::sin(float(block*257+i)*0.03f):0.0f);
            buffer.setSample(1,i,0);
        }
        p.processBlock(buffer,midi);midi.clear();
        if(block>100) peak=std::max(peak,buffer.getMagnitude(0,257));
    }
    return peak;
}
int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    SMU2000AudioProcessor p;
    auto layout=p.getBusesLayout();
    layout.outputBuses.set(0,juce::AudioChannelSet::mono());
    if(p.isBusesLayoutSupported(layout)) return 5; // Reject ambiguous stereo-in/mono-out.
    layout.inputBuses.set(0,juce::AudioChannelSet::mono());
    if(!p.isBusesLayoutSupported(layout)) return 6;
    layout.inputBuses.set(0,juce::AudioChannelSet::disabled());
    if(!p.isBusesLayoutSupported(layout)) return 7;
    p.prepareToPlay(44100,257);
    p.inputGain->setValueNotifyingHost(0.5f);
    float peak=render(p,true,true);
    std::cout<<"A/D input peak="<<peak<<std::endl;
    if(peak<0.001f) return 1;
    juce::MemoryBlock state; p.getStateInformation(state);
    p.setStateInformation(state.getData(),int(state.getSize()));
    peak=render(p,true,false);
    std::cout<<"restored A/D input peak="<<peak<<std::endl;
    if(peak<0.001f) return 2;
    p.inputGain->setValueNotifyingHost(0);
    if(render(p,true,false)>0.0001f) return 3;
    for(double rate : {48000.0,96000.0}) {
        p.prepareToPlay(rate,257); p.inputGain->setValueNotifyingHost(0.5f);
        if(render(p,true,false)<0.001f) return 4;
    }
    std::cout<<"PASS: host input reaches the A/D part, restored input plays, volume mutes"<<std::endl;
}
