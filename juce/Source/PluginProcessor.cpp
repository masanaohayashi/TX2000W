/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

bool SMU2000AudioProcessor::loadSequenceFile(const juce::File& file,juce::String& error)
{
    std::string message;
    try {
        if (!engine.sequence().load(file.getFullPathName().toStdString(),message)) {
            error=juce::String::fromUTF8(message.c_str()); return false;
        }
    } catch (const std::exception& e) {
        error=juce::String::fromUTF8(e.what()); return false;
    }
    sequenceFileName=file.getFileName();
    return true;
}

bool SMU2000AudioProcessor::loadSequenceFiles(const juce::Array<juce::File>& files, juce::String& error)
{
    if(files.isEmpty()) return false;
    if(sequenceFiles.isEmpty()) {
        if(!loadSequenceFile(files.getFirst(),error)) return false;
        sequenceIndex=0;
    }
    sequenceFiles.addArray(files); sequenceError.clear();
    return true;
}
bool SMU2000AudioProcessor::selectSequence(int index,bool play,juce::String& error)
{
    if(index<0 || index>=sequenceFiles.size()) return false;
    if(!loadSequenceFile(sequenceFiles[index],error)) return false;
    sequenceIndex=index; sequenceError.clear();
    if(play) engine.sequence().play();
    return true;
}
void SMU2000AudioProcessor::timerCallback()
{
    if(engine.sequence().takeFinished() && sequenceIndex>=0 && sequenceIndex+1<sequenceFiles.size())
        selectSequence(sequenceIndex+1,true,sequenceError);
}

//==============================================================================
SMU2000AudioProcessor::SMU2000AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                       .withInput  ("A/D Input", juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    addParameter(outputGain = new juce::AudioParameterFloat(juce::ParameterID{"outputGain",1},
        "Output Volume",0.0f,1.0f,0.72f));
    addParameter(inputGain = new juce::AudioParameterFloat(juce::ParameterID{"inputGain",1},
        "A/D Input Volume",0.0f,1.0f,0.0f));
    startTimerHz(10);
}

SMU2000AudioProcessor::~SMU2000AudioProcessor()
{
    stopTimer();
}

//==============================================================================
const juce::String SMU2000AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SMU2000AudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SMU2000AudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SMU2000AudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SMU2000AudioProcessor::getTailLengthSeconds() const
{
    return 20.0;
}

int SMU2000AudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int SMU2000AudioProcessor::getCurrentProgram()
{
    return 0;
}

void SMU2000AudioProcessor::setCurrentProgram (int index)
{
}

const juce::String SMU2000AudioProcessor::getProgramName (int index)
{
    return {};
}

void SMU2000AudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void SMU2000AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    cpuLoad.reset(sampleRate,juce::jmax(1,samplesPerBlock));
    gain.reset(sampleRate,0.02);
    gain.setCurrentAndTargetValue(outputGain->get());
    engine.prepare(sampleRate,samplesPerBlock);
    setLatencySamples(engine.latencySamples());
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
}

void SMU2000AudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SMU2000AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    const auto input = layouts.getMainInputChannelSet();
    // The in-place engine supports mono-in/stereo-out, but not stereo-in/mono-out.
    if (input.size() > layouts.getMainOutputChannelSet().size()) return false;
    if (!input.isDisabled() && input != juce::AudioChannelSet::mono()
        && input != juce::AudioChannelSet::stereo()) return false;

    return true;
  #endif
}
#endif

void SMU2000AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    const auto started = juce::Time::getMillisecondCounterHiRes();
    juce::ScopedNoDenormals noDenormals;
    engine.process(buffer,midiMessages,getTotalNumInputChannels(),inputGain->get());
    midiMessages.clear();
    engine.takeOutgoing(midiMessages);
    gain.setTargetValue(outputGain->get());
    for (int n=0;n<buffer.getNumSamples();++n)
    {
        const float g=gain.getNextValue();
        for (int ch=0;ch<buffer.getNumChannels();++ch)
            buffer.setSample(ch,n,buffer.getSample(ch,n)*g);
    }
    // Zero-sized host callbacks have no audio deadline to measure.
    if(buffer.getNumSamples()>0)
        cpuLoad.registerRenderTime(juce::Time::getMillisecondCounterHiRes()-started,buffer.getNumSamples());
}

//==============================================================================
bool SMU2000AudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* SMU2000AudioProcessor::createEditor()
{
    return new SMU2000AudioProcessorEditor (*this);
}

//==============================================================================
void SMU2000AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    destData.reset();
    juce::MemoryOutputStream stream(destData,false);
    stream.writeInt(0x4d553233);
    stream.writeFloat(outputGain->get());
    stream.writeFloat(inputGain->get());
    stream.writeInt(editorWidth.load());
    auto state=engine.save();
    stream.write(state.getData(),state.getSize());
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void SMU2000AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (sizeInBytes<8) return;
    juce::MemoryInputStream stream(data,size_t(sizeInBytes),false);
    const int magic = stream.readInt();
    if (magic != 0x4d553231 && magic != 0x4d553232 && magic != 0x4d553233) return;
    const int headerSize = magic == 0x4d553233 ? 16 : magic == 0x4d553232 ? 12 : 8;
    if (sizeInBytes < headerSize) return;
    const float value=stream.readFloat();
    const float inputValue = headerSize >= 12 ? stream.readFloat() : 0.0f;
    if (headerSize == 16) editorWidth = juce::jlimit (MIN_EDITOR_WIDTH, MAX_EDITOR_WIDTH, stream.readInt());
    if (!std::isfinite(value) || !std::isfinite(inputValue)) return;
    outputGain->setValueNotifyingHost(juce::jlimit(0.0f,1.0f,value));
    inputGain->setValueNotifyingHost(juce::jlimit(0.0f,1.0f,inputValue));
    if (sizeInBytes>headerSize) engine.restore(static_cast<const char*>(data)+headerSize,size_t(sizeInBytes-headerSize));
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SMU2000AudioProcessor();
}
