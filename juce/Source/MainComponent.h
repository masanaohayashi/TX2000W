/*
  ==============================================================================

  This is an automatically generated GUI class created by the Projucer!

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Created with Projucer version: 9.0.4

  ------------------------------------------------------------------------------

  The Projucer is part of the JUCE library.
  Copyright (c) - Raw Material Software Limited.

  ==============================================================================
*/

#pragma once

//[Headers]     -- You can add your own extra header files here --
#include <JuceHeader.h>
#include <array>
#include <memory>
#include "OriginalPanel.h"
#include "SettingsComponent.h"
#include "PanelEncoder.h"
class SMU2000AudioProcessor;
#include "AudioSettingsScreen.h"
//[/Headers]



//==============================================================================
/**
                                                                    //[Comments]
    An auto-generated component, created by the Projucer.

    Describe your class and how it works here!
                                                                    //[/Comments]
*/
class MainComponent  : public juce::Component,
                       private juce::Timer,
                       public juce::FileDragAndDropTarget,
                       public juce::Slider::Listener,
                       public juce::Button::Listener
{
public:
    //==============================================================================
    MainComponent ();
    ~MainComponent() override;

    //==============================================================================
    //[UserMethods]     -- You can add your own custom methods in this section.
    void setSnapshot (const OriginalPanel::Snapshot&);
    void connectToProcessor (SMU2000AudioProcessor&);
    void timerCallback() override;
        bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&, int, int) override;
    //[/UserMethods]

    void paint (juce::Graphics& g) override;
    void resized() override;
    void sliderValueChanged (juce::Slider* sliderThatWasMoved) override;
    void buttonClicked (juce::Button* buttonThatWasClicked) override;



private:
    //[UserVariables]   -- You can add your own custom variables in this section.
    OriginalPanel originalPanel;
    SMU2000AudioProcessor* processor = nullptr;
    std::unique_ptr<juce::SliderParameterAttachment> volumeAttachment;
    std::unique_ptr<juce::SliderParameterAttachment> inputAttachment;
    SettingsComponent settings;
    void wireButton (juce::ImageButton&, int index, const juce::String& name);

        std::unique_ptr<juce::FileChooser> sequenceChooser;
    void chooseSequences();
   #if JUCE_IOS
    void chooseRoms();
   #endif
    void showSequenceList();
    void updateSequenceControls();
    void loadSequences(const juce::Array<juce::File>&);
    void stepSequence(int direction);
    static void showSequenceError(const juce::String&);
    std::unique_ptr<AudioSettingsScreen> audioSettingsScreen; // iOS の全面のオーディオ設定
    //[/UserVariables]

    //==============================================================================
    std::unique_ptr<juce::Component> lcd;
    std::unique_ptr<PanelEncoder> dial;
    struct FilmstripSliderLookAndFeel1  : public juce::LookAndFeel_V4
    {
        void setFilmstrip (juce::Image imageToUse, int numFramesToUse, bool verticalLayoutToUse)
        {
            image = imageToUse;
            numFrames = juce::jmax (1, numFramesToUse);
            verticalLayout = verticalLayoutToUse;
        }

        void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                               float sliderPos, float rotaryStartAngle,
                               float rotaryEndAngle, juce::Slider& slider) override
        {
            if (image.isValid() && numFrames > 1)
            {
                auto frame = juce::jlimit (0, numFrames - 1,
                                          juce::roundToInt (sliderPos * (float) (numFrames - 1)));

                if (verticalLayout)
                {
                    auto frameHeight = image.getHeight() / numFrames;

                    if (frameHeight > 0)
                        g.drawImage (image, x, y, width, height,
                                     0, frame * frameHeight, image.getWidth(), frameHeight);
                }
                else
                {
                    auto frameWidth = image.getWidth() / numFrames;

                    if (frameWidth > 0)
                        g.drawImage (image, x, y, width, height,
                                     frame * frameWidth, 0, frameWidth, image.getHeight());
                }

                return;
            }

            juce::LookAndFeel_V4::drawRotarySlider (g, x, y, width, height, sliderPos,
                                                    rotaryStartAngle, rotaryEndAngle, slider);
        }

        juce::Image image;
        int numFrames = 1;
        bool verticalLayout = true;
    };

    FilmstripSliderLookAndFeel1 filmstripSliderLookAndFeel1;
    std::unique_ptr<juce::ImageButton> buttonPlay;
    std::unique_ptr<juce::ImageButton> buttonEdit;
    std::unique_ptr<juce::ImageButton> buttonUtil;
    std::unique_ptr<juce::ImageButton> buttonEffect;
    std::unique_ptr<juce::ImageButton> buttonMode;
    std::unique_ptr<juce::ImageButton> buttonEQ;
    std::unique_ptr<juce::ImageButton> buttonMuteSolo;
    std::unique_ptr<juce::ImageButton> buttonPartDec;
    std::unique_ptr<juce::ImageButton> buttonPartInc;
    std::unique_ptr<juce::ImageButton> buttonEnter;
    std::unique_ptr<juce::ImageButton> buttonSelectDec;
    std::unique_ptr<juce::ImageButton> buttonSelectInc;
    std::unique_ptr<juce::ImageButton> buttonExit;
    std::unique_ptr<juce::ImageButton> buttonValueDec;
    std::unique_ptr<juce::ImageButton> buttonValueInc;
    std::unique_ptr<juce::Slider> sliderMasterVolume;
    struct FilmstripSliderLookAndFeel2  : public juce::LookAndFeel_V4
    {
        void setFilmstrip (juce::Image imageToUse, int numFramesToUse, bool verticalLayoutToUse)
        {
            image = imageToUse;
            numFrames = juce::jmax (1, numFramesToUse);
            verticalLayout = verticalLayoutToUse;
        }

        void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                               float sliderPos, float rotaryStartAngle,
                               float rotaryEndAngle, juce::Slider& slider) override
        {
            if (image.isValid() && numFrames > 1)
            {
                auto frame = juce::jlimit (0, numFrames - 1,
                                          juce::roundToInt (sliderPos * (float) (numFrames - 1)));

                if (verticalLayout)
                {
                    auto frameHeight = image.getHeight() / numFrames;

                    if (frameHeight > 0)
                        g.drawImage (image, x, y, width, height,
                                     0, frame * frameHeight, image.getWidth(), frameHeight);
                }
                else
                {
                    auto frameWidth = image.getWidth() / numFrames;

                    if (frameWidth > 0)
                        g.drawImage (image, x, y, width, height,
                                     frame * frameWidth, 0, frameWidth, image.getHeight());
                }

                return;
            }

            juce::LookAndFeel_V4::drawRotarySlider (g, x, y, width, height, sliderPos,
                                                    rotaryStartAngle, rotaryEndAngle, slider);
        }

        juce::Image image;
        int numFrames = 1;
        bool verticalLayout = true;
    };

    FilmstripSliderLookAndFeel2 filmstripSliderLookAndFeel2;
    std::unique_ptr<juce::Slider> sliderAudioInVolume;
    struct FilmstripSliderLookAndFeel3  : public juce::LookAndFeel_V4
    {
        void setFilmstrip (juce::Image imageToUse, int numFramesToUse, bool verticalLayoutToUse)
        {
            image = imageToUse;
            numFrames = juce::jmax (1, numFramesToUse);
            verticalLayout = verticalLayoutToUse;
        }

        void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                               float sliderPos, float rotaryStartAngle,
                               float rotaryEndAngle, juce::Slider& slider) override
        {
            if (image.isValid() && numFrames > 1)
            {
                auto frame = juce::jlimit (0, numFrames - 1,
                                          juce::roundToInt (sliderPos * (float) (numFrames - 1)));

                if (verticalLayout)
                {
                    auto frameHeight = image.getHeight() / numFrames;

                    if (frameHeight > 0)
                        g.drawImage (image, x, y, width, height,
                                     0, frame * frameHeight, image.getWidth(), frameHeight);
                }
                else
                {
                    auto frameWidth = image.getWidth() / numFrames;

                    if (frameWidth > 0)
                        g.drawImage (image, x, y, width, height,
                                     frame * frameWidth, 0, frameWidth, image.getHeight());
                }

                return;
            }

            juce::LookAndFeel_V4::drawRotarySlider (g, x, y, width, height, sliderPos,
                                                    rotaryStartAngle, rotaryEndAngle, slider);
        }

        juce::Image image;
        int numFrames = 1;
        bool verticalLayout = true;
    };

    FilmstripSliderLookAndFeel3 filmstripSliderLookAndFeel3;
    std::unique_ptr<juce::ImageButton> buttonSettings;
    std::unique_ptr<juce::ImageButton> buttonAudition;
    std::unique_ptr<juce::TextButton> buttonLoad;
    std::unique_ptr<juce::TextButton> buttonPlayPause;
    std::unique_ptr<juce::TextButton> buttonStop;
    std::unique_ptr<juce::TextButton> buttonPrev;
    std::unique_ptr<juce::TextButton> buttonNext;
    std::unique_ptr<juce::TextButton> buttonList;
    std::unique_ptr<juce::Label> labelCpu;
    juce::Image cachedImage_BinaryData_TX2000W_png_4;


    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

//[EndFile] You can add extra defines here...
//[/EndFile]

