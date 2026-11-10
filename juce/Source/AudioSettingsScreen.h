#pragma once
#include <JuceHeader.h>
#include <functional>
#include <memory>

// iOS には窓の枠がないので、オーディオ設定は窓の代わりに画面全体へ重ねて出す。
class AudioSettingsScreen : public juce::Component
{
public:
    AudioSettingsScreen (juce::AudioDeviceManager&, int maxInputs, int maxOutputs, bool showMidiOutput,
                         juce::Value* muteInput, std::function<void()> onClose);

    // Standalone でだけ作れる。プラグインでは nullptr
    static std::unique_ptr<AudioSettingsScreen> forStandalone (std::function<void()> onClose);

    std::function<void()> onOpenRoms;
    void setStatus (const juce::String& text) { statusLabel.setText (text, juce::dontSendNotification); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentSizeChanged() override;

private:
    class Content : public juce::Component
    {
    public:
        Content (juce::AudioDeviceManager&, int maxInputs, int maxOutputs, bool showMidiOutput, juce::Value* muteInput);
        void resized() override;
        void childBoundsChanged (juce::Component*) override;

    private:
        juce::ToggleButton muteButton { "Mute audio input" };
        juce::AudioDeviceSelectorComponent selector;
    };

    juce::TextButton closeButton { "Close" }, romButton { "Open ROM Folder" };
    juce::Label statusLabel;
    juce::Viewport viewport;
    Content content;
};
