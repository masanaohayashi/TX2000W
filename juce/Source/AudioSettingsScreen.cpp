#include "AudioSettingsScreen.h"
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace
{
constexpr int ITEM_HEIGHT = 32, CONTENT_WIDTH = 640, MARGIN = 16, CLOSE_WIDTH = 96, CLOSE_HEIGHT = 44, ROM_WIDTH = 180;
}

AudioSettingsScreen::Content::Content (juce::AudioDeviceManager& manager, int maxInputs, int maxOutputs,
                                       bool showMidiOutput, juce::Value* muteInput)
    : selector (manager, 0, maxInputs, 0, maxOutputs, true, showMidiOutput, true, false)
{
    selector.setItemHeight (ITEM_HEIGHT);
    addAndMakeVisible (selector);
    if (muteInput != nullptr)
    {
        muteButton.setClickingTogglesState (true);
        muteButton.getToggleStateValue().referTo (*muteInput);
        addAndMakeVisible (muteButton);
    }
}

void AudioSettingsScreen::Content::resized()
{
    const int top = muteButton.isVisible() ? ITEM_HEIGHT + MARGIN : 0;
    muteButton.setBounds (0, 0, getWidth(), ITEM_HEIGHT);
    selector.setBounds (0, top, getWidth(), selector.getHeight());
    // 選ぶものが変わると selector の高さが変わる。Viewport はこの高さでスクロールする
    if (getHeight() != selector.getBottom())
        setSize (getWidth(), selector.getBottom());
}

void AudioSettingsScreen::Content::childBoundsChanged (juce::Component*)
{
    if (getHeight() != selector.getBottom())
        setSize (getWidth(), selector.getBottom());
}

AudioSettingsScreen::AudioSettingsScreen (juce::AudioDeviceManager& manager, int maxInputs, int maxOutputs,
                                          bool showMidiOutput, juce::Value* muteInput, std::function<void()> onClose)
    : content (manager, maxInputs, maxOutputs, showMidiOutput, muteInput)
{
    closeButton.setComponentID ("closeAudioSettings");
    closeButton.onClick = std::move (onClose);
    addAndMakeVisible (closeButton);
    romButton.onClick = [this] { if (onOpenRoms) onOpenRoms(); };
    addAndMakeVisible (romButton);
    addAndMakeVisible (statusLabel);
    viewport.setComponentID ("audioSettingsViewport");
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::all);
    viewport.setViewedComponent (&content, false);
    addAndMakeVisible (viewport);
}

std::unique_ptr<AudioSettingsScreen> AudioSettingsScreen::forStandalone (std::function<void()> onClose)
{
    auto* holder = juce::StandalonePluginHolder::getInstance();
    if (holder == nullptr || holder->processor == nullptr)
        return {};
    auto& processor = *holder->processor;
    const auto channels = [] (juce::AudioProcessor::Bus* bus) { return bus != nullptr ? bus->getDefaultLayout().size() : 0; };
    return std::make_unique<AudioSettingsScreen> (holder->deviceManager, channels (processor.getBus (true, 0)),
                                                  channels (processor.getBus (false, 0)), processor.producesMidi(),
                                                  holder->getProcessorHasPotentialFeedbackLoop() ? &holder->getMuteInputValue() : nullptr,
                                                  std::move (onClose));
}

void AudioSettingsScreen::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff121212));
}

void AudioSettingsScreen::resized()
{
    auto area = getLocalBounds();
    if (auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds()))
        area = display->safeAreaInsets.subtractedFrom (area);
    area.reduce (MARGIN, MARGIN);

    auto bar = area.removeFromTop (CLOSE_HEIGHT);
    closeButton.setBounds (bar.removeFromRight (CLOSE_WIDTH));
    romButton.setBounds (bar.removeFromLeft (ROM_WIDTH));
    statusLabel.setBounds (bar.reduced (MARGIN, 0));
    area.removeFromTop (MARGIN);
    viewport.setBounds (area.withSizeKeepingCentre (juce::jmin (CONTENT_WIDTH, area.getWidth()), area.getHeight()));
    content.setSize (viewport.getMaximumVisibleWidth(), content.getHeight());
}

void AudioSettingsScreen::parentSizeChanged()
{
    if (auto* parent = getParentComponent())
        setBounds (parent->getLocalBounds()); // 画面を回したとき
}
