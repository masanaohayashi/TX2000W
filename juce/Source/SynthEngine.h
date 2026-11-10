#pragma once
#include <JuceHeader.h>
#include "PanelSnapshot.h"
#include "Sequence/SequencePlayer.h"

// Playback runs synchronously in processBlock. Non-RT prepare/save/restore use
// exclusive handoff; the audio callback never waits for them.
class SynthEngine
{
public:
    SynthEngine();
    ~SynthEngine();
    void prepare (double rate, int blockSize);
    int latencySamples() const;
    SequencePlayer& sequence();
    void process (juce::AudioBuffer<float>&, const juce::MidiBuffer&, int inputChannels = 0, float inputGain = 0);
    bool isReady() const;
    juce::String status() const;
   #if JUCE_IOS
    static bool copyRomFolder (const juce::URL& folder, juce::String& error); // 選んだ ROM をアプリへコピーする。時間がかかるので別のスレッドで
    bool reloadRoms (juce::String& error); // message thread
   #endif
    bool readPanel (PanelSnapshot&); // message thread only: generates LCD glyphs
    void button (int index, bool down);
    void turn (int delta);
    void takeOutgoing (juce::MidiBuffer&); // audio thread: the panel's bulk dump
    juce::MemoryBlock save();
    void restore (const void*, size_t);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
