/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "SynthEngine.h"

//==============================================================================
/**
*/
class SMU2000AudioProcessor  : public juce::AudioProcessor, private juce::Timer
{
public:
    //==============================================================================
    SMU2000AudioProcessor();
    ~SMU2000AudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
public:
    SynthEngine engine;
    double getCpuLoadPercent() const { return cpuLoad.getLoadAsPercentage(); }
    bool loadSequenceFile(const juce::File&, juce::String& error);
    static constexpr int MIN_EDITOR_WIDTH=1024, MAX_EDITOR_WIDTH=4096;
    std::atomic<int> editorWidth{MIN_EDITOR_WIDTH}; // 高さは比率で決まる。ステートに残す
    juce::String sequenceFileName; // message-thread UI metadata
    juce::Array<juce::File> sequenceFiles;
    int sequenceIndex = -1;
    juce::String sequenceError;
    bool loadSequenceFiles(const juce::Array<juce::File>&, juce::String& error);
    bool selectSequence(int index, bool play, juce::String& error);
    void timerCallback() override;
    juce::AudioParameterFloat* outputGain = nullptr;
    juce::AudioParameterFloat* inputGain = nullptr;
private:
    juce::AudioProcessLoadMeasurer cpuLoad;
    juce::SmoothedValue<float> gain;
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SMU2000AudioProcessor)
};
