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

//[Headers] You can add your own extra header files here...
#include "../JuceLibraryCode/BinaryData.h"
#include <algorithm>
#include <cmath>
#include "PluginProcessor.h"
//[/Headers]

#include "MainComponent.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
MainComponent::MainComponent ()
{
    //[Constructor_pre] You can add your own custom stuff here..
    //[/Constructor_pre]

    lcd.reset (new juce::Component());
    addAndMakeVisible (lcd.get());

    lcd->setBounds (350, 18, 187, 59);

    dial.reset (new PanelEncoder (juce::String()));
    addAndMakeVisible (dial.get());
    dial->setRange (0, 10, 0);
    dial->setSliderStyle (juce::Slider::RotaryVerticalDrag);
    dial->setTextBoxStyle (juce::Slider::NoTextBox, false, 80, 20);
    dial->addListener (this);
    filmstripSliderLookAndFeel1.setFilmstrip (juce::ImageCache::getFromMemory (BinaryData::Dial_png, BinaryData::Dial_pngSize), 24, true);
    dial->setLookAndFeel (&filmstripSliderLookAndFeel1);

    dial->setBounds (748, 12, 72, 72);

    buttonPlay.reset (new juce::ImageButton (juce::String()));
    addAndMakeVisible (buttonPlay.get());
    buttonPlay->setButtonText (TRANS ("new button"));
    buttonPlay->addListener (this);

    buttonPlay->setImages (false, true, true,
                           juce::ImageCache::getFromMemory (BinaryData::LedButtonOff_Normal_png, BinaryData::LedButtonOff_Normal_pngSize), 1.000f, juce::Colour (0x00000000),
                           juce::ImageCache::getFromMemory (BinaryData::LedButtonOff_Over_png, BinaryData::LedButtonOff_Over_pngSize), 1.000f, juce::Colour (0x00000000),
                           juce::ImageCache::getFromMemory (BinaryData::LedButtonOff_Down_png, BinaryData::LedButtonOff_Down_pngSize), 1.000f, juce::Colour (0x00000000));
    buttonPlay->setBounds (616, 22, 13, 13);

    buttonEdit.reset (new juce::ImageButton (juce::String()));
    addAndMakeVisible (buttonEdit.get());
    buttonEdit->setButtonText (TRANS ("new button"));
    buttonEdit->addListener (this);

    buttonEdit->setImages (false, true, true,
                           juce::ImageCache::getFromMemory (BinaryData::LedButtonOff_Normal_png, BinaryData::LedButtonOff_Normal_pngSize), 1.000f, juce::Colour (0x00000000),
                           juce::ImageCache::getFromMemory (BinaryData::LedButtonOff_Over_png, BinaryData::LedButtonOff_Over_pngSize), 1.000f, juce::Colour (0x00000000),
                           juce::ImageCache::getFromMemory (BinaryData::LedButtonOff_Down_png, BinaryData::LedButtonOff_Down_pngSize), 1.000f, juce::Colour (0x00000000));
    buttonEdit->setBounds (636, 22, 13, 13);

    buttonUtil.reset (new juce::ImageButton (juce::String()));
    addAndMakeVisible (buttonUtil.get());
    buttonUtil->setButtonText (TRANS ("new button"));
    buttonUtil->addListener (this);

    buttonUtil->setImages (false, true, true,
                           juce::ImageCache::getFromMemory (BinaryData::LedButtonOff_Normal_png, BinaryData::LedButtonOff_Normal_pngSize), 1.000f, juce::Colour (0x00000000),
                           juce::ImageCache::getFromMemory (BinaryData::LedButtonOff_Over_png, BinaryData::LedButtonOff_Over_pngSize), 1.000f, juce::Colour (0x00000000),
                           juce::ImageCache::getFromMemory (BinaryData::LedButtonOff_Down_png, BinaryData::LedButtonOff_Down_pngSize), 1.000f, juce::Colour (0x00000000));
    buttonUtil->setBounds (616, 44, 13, 13);

    buttonEffect.reset (new juce::ImageButton (juce::String()));
    addAndMakeVisible (buttonEffect.get());
    buttonEffect->setButtonText (TRANS ("new button"));
    buttonEffect->addListener (this);

    buttonEffect->setImages (false, true, true,
                             juce::ImageCache::getFromMemory (BinaryData::LedButtonOff_Normal_png, BinaryData::LedButtonOff_Normal_pngSize), 1.000f, juce::Colour (0x00000000),
                             juce::ImageCache::getFromMemory (BinaryData::LedButtonOff_Over_png, BinaryData::LedButtonOff_Over_pngSize), 1.000f, juce::Colour (0x00000000),
                             juce::ImageCache::getFromMemory (BinaryData::LedButtonOff_Down_png, BinaryData::LedButtonOff_Down_pngSize), 1.000f, juce::Colour (0x00000000));
    buttonEffect->setBounds (636, 44, 13, 13);

    buttonMode.reset (new juce::ImageButton (juce::String()));
    addAndMakeVisible (buttonMode.get());
    buttonMode->setButtonText (TRANS ("new button"));
    buttonMode->addListener (this);

    buttonMode->setImages (false, true, true,
                           juce::ImageCache::getFromMemory (BinaryData::LedButtonOff_Normal_png, BinaryData::LedButtonOff_Normal_pngSize), 1.000f, juce::Colour (0x00000000),
                           juce::ImageCache::getFromMemory (BinaryData::LedButtonOff_Over_png, BinaryData::LedButtonOff_Over_pngSize), 1.000f, juce::Colour (0x00000000),
                           juce::ImageCache::getFromMemory (BinaryData::LedButtonOff_Down_png, BinaryData::LedButtonOff_Down_pngSize), 1.000f, juce::Colour (0x00000000));
    buttonMode->setBounds (616, 66, 13, 13);

    buttonEQ.reset (new juce::ImageButton (juce::String()));
    addAndMakeVisible (buttonEQ.get());
    buttonEQ->setButtonText (TRANS ("new button"));
    buttonEQ->addListener (this);

    buttonEQ->setImages (false, true, true,
                         juce::ImageCache::getFromMemory (BinaryData::LedButtonOff_Normal_png, BinaryData::LedButtonOff_Normal_pngSize), 1.000f, juce::Colour (0x00000000),
                         juce::ImageCache::getFromMemory (BinaryData::LedButtonOff_Over_png, BinaryData::LedButtonOff_Over_pngSize), 1.000f, juce::Colour (0x00000000),
                         juce::ImageCache::getFromMemory (BinaryData::LedButtonOff_Down_png, BinaryData::LedButtonOff_Down_pngSize), 1.000f, juce::Colour (0x00000000));
    buttonEQ->setBounds (636, 66, 13, 13);

    buttonMuteSolo.reset (new juce::ImageButton (juce::String()));
    addAndMakeVisible (buttonMuteSolo.get());
    buttonMuteSolo->setButtonText (TRANS ("new button"));
    buttonMuteSolo->addListener (this);

    buttonMuteSolo->setImages (false, true, true,
                               juce::ImageCache::getFromMemory (BinaryData::Button_Down_png, BinaryData::Button_Down_pngSize), 1.000f, juce::Colour (0x00000000),
                               juce::ImageCache::getFromMemory (BinaryData::Button_Over_png, BinaryData::Button_Over_pngSize), 1.000f, juce::Colour (0x00000000),
                               juce::ImageCache::getFromMemory (BinaryData::Button_Down_png, BinaryData::Button_Down_pngSize), 1.000f, juce::Colour (0x00000000));
    buttonMuteSolo->setBounds (668, 21, 16, 16);

    buttonPartDec.reset (new juce::ImageButton (juce::String()));
    addAndMakeVisible (buttonPartDec.get());
    buttonPartDec->setButtonText (TRANS ("new button"));
    buttonPartDec->addListener (this);

    buttonPartDec->setImages (false, true, true,
                              juce::ImageCache::getFromMemory (BinaryData::Button_Down_png, BinaryData::Button_Down_pngSize), 1.000f, juce::Colour (0x00000000),
                              juce::ImageCache::getFromMemory (BinaryData::Button_Over_png, BinaryData::Button_Over_pngSize), 1.000f, juce::Colour (0x00000000),
                              juce::ImageCache::getFromMemory (BinaryData::Button_Down_png, BinaryData::Button_Down_pngSize), 1.000f, juce::Colour (0x00000000));
    buttonPartDec->setBounds (689, 21, 16, 16);

    buttonPartInc.reset (new juce::ImageButton (juce::String()));
    addAndMakeVisible (buttonPartInc.get());
    buttonPartInc->setButtonText (TRANS ("new button"));
    buttonPartInc->addListener (this);

    buttonPartInc->setImages (false, true, true,
                              juce::ImageCache::getFromMemory (BinaryData::Button_Down_png, BinaryData::Button_Down_pngSize), 1.000f, juce::Colour (0x00000000),
                              juce::ImageCache::getFromMemory (BinaryData::Button_Over_png, BinaryData::Button_Over_pngSize), 1.000f, juce::Colour (0x00000000),
                              juce::ImageCache::getFromMemory (BinaryData::Button_Down_png, BinaryData::Button_Down_pngSize), 1.000f, juce::Colour (0x00000000));
    buttonPartInc->setBounds (710, 21, 16, 16);

    buttonEnter.reset (new juce::ImageButton (juce::String()));
    addAndMakeVisible (buttonEnter.get());
    buttonEnter->setButtonText (TRANS ("new button"));
    buttonEnter->addListener (this);

    buttonEnter->setImages (false, true, true,
                            juce::ImageCache::getFromMemory (BinaryData::Button_Down_png, BinaryData::Button_Down_pngSize), 1.000f, juce::Colour (0x00000000),
                            juce::ImageCache::getFromMemory (BinaryData::Button_Over_png, BinaryData::Button_Over_pngSize), 1.000f, juce::Colour (0x00000000),
                            juce::ImageCache::getFromMemory (BinaryData::Button_Down_png, BinaryData::Button_Down_pngSize), 1.000f, juce::Colour (0x00000000));
    buttonEnter->setBounds (668, 43, 16, 16);

    buttonSelectDec.reset (new juce::ImageButton (juce::String()));
    addAndMakeVisible (buttonSelectDec.get());
    buttonSelectDec->setButtonText (TRANS ("new button"));
    buttonSelectDec->addListener (this);

    buttonSelectDec->setImages (false, true, true,
                                juce::ImageCache::getFromMemory (BinaryData::Button_Down_png, BinaryData::Button_Down_pngSize), 1.000f, juce::Colour (0x00000000),
                                juce::ImageCache::getFromMemory (BinaryData::Button_Over_png, BinaryData::Button_Over_pngSize), 1.000f, juce::Colour (0x00000000),
                                juce::ImageCache::getFromMemory (BinaryData::Button_Down_png, BinaryData::Button_Down_pngSize), 1.000f, juce::Colour (0x00000000));
    buttonSelectDec->setBounds (689, 43, 16, 16);

    buttonSelectInc.reset (new juce::ImageButton (juce::String()));
    addAndMakeVisible (buttonSelectInc.get());
    buttonSelectInc->setButtonText (TRANS ("new button"));
    buttonSelectInc->addListener (this);

    buttonSelectInc->setImages (false, true, true,
                                juce::ImageCache::getFromMemory (BinaryData::Button_Down_png, BinaryData::Button_Down_pngSize), 1.000f, juce::Colour (0x00000000),
                                juce::ImageCache::getFromMemory (BinaryData::Button_Over_png, BinaryData::Button_Over_pngSize), 1.000f, juce::Colour (0x00000000),
                                juce::ImageCache::getFromMemory (BinaryData::Button_Down_png, BinaryData::Button_Down_pngSize), 1.000f, juce::Colour (0x00000000));
    buttonSelectInc->setBounds (710, 43, 16, 16);

    buttonExit.reset (new juce::ImageButton (juce::String()));
    addAndMakeVisible (buttonExit.get());
    buttonExit->setButtonText (TRANS ("new button"));
    buttonExit->addListener (this);

    buttonExit->setImages (false, true, true,
                           juce::ImageCache::getFromMemory (BinaryData::Button_Down_png, BinaryData::Button_Down_pngSize), 1.000f, juce::Colour (0x00000000),
                           juce::ImageCache::getFromMemory (BinaryData::Button_Over_png, BinaryData::Button_Over_pngSize), 1.000f, juce::Colour (0x00000000),
                           juce::ImageCache::getFromMemory (BinaryData::Button_Down_png, BinaryData::Button_Down_pngSize), 1.000f, juce::Colour (0x00000000));
    buttonExit->setBounds (668, 65, 16, 16);

    buttonValueDec.reset (new juce::ImageButton (juce::String()));
    addAndMakeVisible (buttonValueDec.get());
    buttonValueDec->setButtonText (TRANS ("new button"));
    buttonValueDec->addListener (this);

    buttonValueDec->setImages (false, true, true,
                               juce::ImageCache::getFromMemory (BinaryData::Button_Down_png, BinaryData::Button_Down_pngSize), 1.000f, juce::Colour (0x00000000),
                               juce::ImageCache::getFromMemory (BinaryData::Button_Over_png, BinaryData::Button_Over_pngSize), 1.000f, juce::Colour (0x00000000),
                               juce::ImageCache::getFromMemory (BinaryData::Button_Down_png, BinaryData::Button_Down_pngSize), 1.000f, juce::Colour (0x00000000));
    buttonValueDec->setBounds (689, 65, 16, 16);

    buttonValueInc.reset (new juce::ImageButton (juce::String()));
    addAndMakeVisible (buttonValueInc.get());
    buttonValueInc->setButtonText (TRANS ("new button"));
    buttonValueInc->addListener (this);

    buttonValueInc->setImages (false, true, true,
                               juce::ImageCache::getFromMemory (BinaryData::Button_Down_png, BinaryData::Button_Down_pngSize), 1.000f, juce::Colour (0x00000000),
                               juce::ImageCache::getFromMemory (BinaryData::Button_Over_png, BinaryData::Button_Over_pngSize), 1.000f, juce::Colour (0x00000000),
                               juce::ImageCache::getFromMemory (BinaryData::Button_Down_png, BinaryData::Button_Down_pngSize), 1.000f, juce::Colour (0x00000000));
    buttonValueInc->setBounds (710, 65, 16, 16);

    sliderMasterVolume.reset (new juce::Slider (juce::String()));
    addAndMakeVisible (sliderMasterVolume.get());
    sliderMasterVolume->setRange (0, 10, 0);
    sliderMasterVolume->setSliderStyle (juce::Slider::RotaryVerticalDrag);
    sliderMasterVolume->setTextBoxStyle (juce::Slider::NoTextBox, false, 80, 20);
    sliderMasterVolume->addListener (this);
    filmstripSliderLookAndFeel2.setFilmstrip (juce::ImageCache::getFromMemory (BinaryData::Knob_png, BinaryData::Knob_pngSize), 101, true);
    sliderMasterVolume->setLookAndFeel (&filmstripSliderLookAndFeel2);

    sliderMasterVolume->setBounds (153, 31, 24, 24);

    sliderAudioInVolume.reset (new juce::Slider (juce::String()));
    addAndMakeVisible (sliderAudioInVolume.get());
    sliderAudioInVolume->setRange (0, 10, 0);
    sliderAudioInVolume->setSliderStyle (juce::Slider::RotaryVerticalDrag);
    sliderAudioInVolume->setTextBoxStyle (juce::Slider::NoTextBox, false, 80, 20);
    sliderAudioInVolume->addListener (this);
    filmstripSliderLookAndFeel3.setFilmstrip (juce::ImageCache::getFromMemory (BinaryData::Knob_png, BinaryData::Knob_pngSize), 101, true);
    sliderAudioInVolume->setLookAndFeel (&filmstripSliderLookAndFeel3);

    sliderAudioInVolume->setBounds (285, 31, 24, 24);

    buttonSettings.reset (new juce::ImageButton (juce::String()));
    addAndMakeVisible (buttonSettings.get());
    buttonSettings->addListener (this);

    buttonSettings->setImages (false, true, true,
                               juce::ImageCache::getFromMemory (BinaryData::Power_normal_png, BinaryData::Power_normal_pngSize), 1.000f, juce::Colour (0x00000000),
                               juce::ImageCache::getFromMemory (BinaryData::Power_over_png, BinaryData::Power_over_pngSize), 1.000f, juce::Colour (0x00000000),
                               juce::ImageCache::getFromMemory (BinaryData::Power_down_png, BinaryData::Power_down_pngSize), 1.000f, juce::Colour (0x00000000));
    buttonSettings->setBounds (890, 28, 36, 36);

    buttonAudition.reset (new juce::ImageButton (juce::String()));
    addAndMakeVisible (buttonAudition.get());
    buttonAudition->addListener (this);

    buttonAudition->setImages (false, true, true,
                               juce::Image(), 1.000f, juce::Colour (0x00000000),
                               juce::Image(), 1.000f, juce::Colour (0x00000000),
                               juce::Image(), 1.000f, juce::Colour (0x00000000));
    buttonAudition->setBounds (108, 31, 28, 28);

    buttonLoad.reset (new juce::TextButton (juce::String()));
    addAndMakeVisible (buttonLoad.get());
    buttonLoad->setButtonText (TRANS ("LOAD"));
    buttonLoad->setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight | juce::Button::ConnectedOnTop | juce::Button::ConnectedOnBottom);
    buttonLoad->addListener (this);
    buttonLoad->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff121212));
    buttonLoad->setColour (juce::TextButton::textColourOffId, juce::Colours::white);

    buttonLoad->setBounds (128, 74, 40, 16);

    buttonPlayPause.reset (new juce::TextButton (juce::String()));
    addAndMakeVisible (buttonPlayPause.get());
    buttonPlayPause->setButtonText (TRANS ("PLAY"));
    buttonPlayPause->setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight | juce::Button::ConnectedOnTop | juce::Button::ConnectedOnBottom);
    buttonPlayPause->addListener (this);
    buttonPlayPause->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff121212));

    buttonPlayPause->setBounds (84, 74, 40, 16);

    buttonStop.reset (new juce::TextButton (juce::String()));
    addAndMakeVisible (buttonStop.get());
    buttonStop->setButtonText (TRANS ("STOP"));
    buttonStop->setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight | juce::Button::ConnectedOnTop | juce::Button::ConnectedOnBottom);
    buttonStop->addListener (this);
    buttonStop->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff121212));

    buttonStop->setBounds (40, 74, 40, 16);

    buttonPrev.reset (new juce::TextButton (juce::String()));
    addAndMakeVisible (buttonPrev.get());
    buttonPrev->setButtonText (TRANS ("|<"));
    buttonPrev->setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight | juce::Button::ConnectedOnTop | juce::Button::ConnectedOnBottom);
    buttonPrev->addListener (this);
    buttonPrev->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff121212));
    buttonPrev->setColour (juce::TextButton::textColourOffId, juce::Colours::white);

    buttonPrev->setBounds (172, 74, 16, 16);

    buttonNext.reset (new juce::TextButton (juce::String()));
    addAndMakeVisible (buttonNext.get());
    buttonNext->setButtonText (TRANS (">|"));
    buttonNext->setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight | juce::Button::ConnectedOnTop | juce::Button::ConnectedOnBottom);
    buttonNext->addListener (this);
    buttonNext->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff121212));
    buttonNext->setColour (juce::TextButton::textColourOffId, juce::Colours::white);

    buttonNext->setBounds (308, 74, 16, 16);

    buttonList.reset (new juce::TextButton (juce::String()));
    addAndMakeVisible (buttonList.get());
    buttonList->setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight | juce::Button::ConnectedOnTop | juce::Button::ConnectedOnBottom);
    buttonList->addListener (this);
    buttonList->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff121212));
    buttonList->setColour (juce::TextButton::textColourOffId, juce::Colours::white);

    buttonList->setBounds (192, 74, 112, 16);

    labelCpu.reset (new juce::Label (juce::String(),
                                     TRANS ("0%")));
    addAndMakeVisible (labelCpu.get());
    labelCpu->setFont (juce::Font (juce::FontOptions { 15.00f, juce::Font::plain }.withStyle ("Regular").withMetricsKind (juce::TypefaceMetricsKind::legacy)));
    labelCpu->setJustificationType (juce::Justification::centredRight);
    labelCpu->setEditable (false, false, false);
    labelCpu->setColour (juce::Label::textColourId, juce::Colour (0x80ffffff));
    labelCpu->setColour (juce::TextEditor::textColourId, juce::Colours::black);
    labelCpu->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0x00000000));

    labelCpu->setBounds (960, 32, 40, 24);

    cachedImage_BinaryData_TX2000W_png_4 = juce::ImageCache::getFromMemory (BinaryData::TX2000W_png, BinaryData::TX2000W_pngSize);

    //[UserPreSize]
    //[/UserPreSize]

    setSize (1024, 92);


    //[Constructor] You can add your own custom stuff here..
    lcd->setComponentID ("lcd");
    lcd->setInterceptsMouseClicks (false, false);
    dial->setComponentID ("dial");
    dial->setRange (0, 23, 1);
    dial->onStep = [this] (int steps) { if (processor) processor->engine.turn (steps); };
    sliderMasterVolume->setComponentID ("masterVolume");
    sliderAudioInVolume->setComponentID ("audioInputVolume");
    wireButton (*buttonPlay, 0, "PLAY");
    wireButton (*buttonEdit, 1, "EDIT");
    wireButton (*buttonUtil, 2, "UTIL");
    wireButton (*buttonEffect, 3, "EFFECT");
    wireButton (*buttonMuteSolo, 6, "MUTE / SOLO");
    wireButton (*buttonPartDec, 7, "PART -");
    wireButton (*buttonPartInc, 8, "PART +");
    wireButton (*buttonEnter, 9, "ENTER");
    wireButton (*buttonSelectDec, 10, "SELECT -");
    wireButton (*buttonSelectInc, 11, "SELECT +");
    wireButton (*buttonExit, 12, "EXIT");
    wireButton (*buttonValueDec, 13, "VALUE -");
    wireButton (*buttonValueInc, 14, "VALUE +");
    wireButton (*buttonMode, 4, "MODE");
    wireButton (*buttonEQ, 5, "EQ");
    // 絵の PHONES の所。押しているあいだ、いまのパートを UTIL > SYS の Audition の音で鳴らす
    wireButton (*buttonAudition, 15, "AUDITION");
    buttonSettings->setComponentID ("settings");
   #if JUCE_IOS || JUCE_ANDROID
    // パネルは細いので、窓の一番上に画面全体で重ねる。プラグインでは設定するものがない
    buttonSettings->onClick = [this]
    {
        audioSettingsScreen = AudioSettingsScreen::forStandalone ([safe = juce::Component::SafePointer<MainComponent> (this)]
        {
            // 閉じるボタンの中で自分を消さないよう、後で消す
            juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->audioSettingsScreen.reset(); });
        });
        if (audioSettingsScreen != nullptr)
        {
           #if JUCE_IOS
            audioSettingsScreen->onOpenRoms = [this] { chooseRoms(); };
            if (processor != nullptr) audioSettingsScreen->setStatus (processor->engine.status());
           #endif
            auto* top = getTopLevelComponent();
            top->addAndMakeVisible (*audioSettingsScreen);
            audioSettingsScreen->setBounds (top->getLocalBounds());
        }
    };
   #else
    buttonSettings->onClick = [this] { settings.setVisible (true); settings.toFront (true); };
   #endif
    settings.setComponentID ("settingsPanel");
    settings.onClose = [this] { settings.setVisible (false); };

    addChildComponent (settings);
    resized();
        buttonLoad->setComponentID("sequenceLoad");
    buttonPlayPause->setComponentID("sequencePlayPause");
    buttonStop->setComponentID("sequenceStop");
    buttonPrev->setComponentID("sequencePrevious");
    labelCpu->setComponentID("cpuLoad");
    labelCpu->setText("0.0%",juce::dontSendNotification);
    buttonNext->setComponentID("sequenceNext");
    buttonList->setComponentID("sequenceList");
    buttonLoad->onClick=[this] {chooseSequences();};
    buttonPlayPause->onClick=[this] {
        if(!processor) return;
        auto& player=processor->engine.sequence();
        if(player.isPlaying()) player.pause(); else player.play();
        updateSequenceControls();
    };
    buttonStop->onClick=[this] {if(processor) processor->engine.sequence().stop();updateSequenceControls();};
    buttonPrev->onClick=[this] {stepSequence(-1);};
    buttonNext->onClick=[this] {stepSequence(1);};
    buttonList->onClick=[this] {showSequenceList();};
    updateSequenceControls();
    //[/Constructor]
}

MainComponent::~MainComponent()
{
    //[Destructor_pre]. You can add your own custom destruction code here..
    stopTimer();
    volumeAttachment.reset();
    inputAttachment.reset();
    if (processor)
        for (int i = 0; i < 35; ++i) processor->engine.button (i, false);
    //[/Destructor_pre]

    lcd = nullptr;
    dial->setLookAndFeel (nullptr);
    dial = nullptr;
    buttonPlay = nullptr;
    buttonEdit = nullptr;
    buttonUtil = nullptr;
    buttonEffect = nullptr;
    buttonMode = nullptr;
    buttonEQ = nullptr;
    buttonMuteSolo = nullptr;
    buttonPartDec = nullptr;
    buttonPartInc = nullptr;
    buttonEnter = nullptr;
    buttonSelectDec = nullptr;
    buttonSelectInc = nullptr;
    buttonExit = nullptr;
    buttonValueDec = nullptr;
    buttonValueInc = nullptr;
    sliderMasterVolume->setLookAndFeel (nullptr);
    sliderMasterVolume = nullptr;
    sliderAudioInVolume->setLookAndFeel (nullptr);
    sliderAudioInVolume = nullptr;
    buttonSettings = nullptr;
    buttonAudition = nullptr;
    buttonLoad = nullptr;
    buttonPlayPause = nullptr;
    buttonStop = nullptr;
    buttonPrev = nullptr;
    buttonNext = nullptr;
    buttonList = nullptr;
    labelCpu = nullptr;


    //[Destructor]. You can add your own custom destruction code here..
    //[/Destructor]
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    //[UserPrePaint] Add your own custom painting code here..
    //[/UserPrePaint]

    g.fillAll (juce::Colour (0xff323e44));

    {
        int x = 0, y = 0, width = 1024, height = 92;
        //[UserPaintCustomArguments] Customize the painting arguments here..
        //[/UserPaintCustomArguments]
        g.setColour (juce::Colours::black);
        g.drawImage (cachedImage_BinaryData_TX2000W_png_4,
                     x, y, width, height,
                     0, 0, cachedImage_BinaryData_TX2000W_png_4.getWidth(), cachedImage_BinaryData_TX2000W_png_4.getHeight());
    }

    //[UserPaint] Add your own custom painting code here..
    juce::Graphics::ScopedSaveState save (g);
    const auto source = originalPanel.rect ("lcd");
    const auto target = lcd->getBounds().toFloat();
    g.reduceClipRegion (lcd->getBounds());
    g.addTransform (juce::AffineTransform::translation (-source.getX(), -source.getY())
        .scaled (target.getWidth() / source.getWidth(), target.getHeight() / source.getHeight())
        .translated (target.getX(), target.getY()));
    originalPanel.drawLCD (g, 1.0f);
    //[/UserPaint]
}

void MainComponent::resized()
{
    //[UserPreResize] Add your own custom resize code here..
    //[/UserPreResize]

    //[UserResized] Add your own custom resize handling here..
    settings.setBounds (getLocalBounds());
    //[/UserResized]
}

void MainComponent::sliderValueChanged (juce::Slider* sliderThatWasMoved)
{
    //[UsersliderValueChanged_Pre]
    //[/UsersliderValueChanged_Pre]

    if (sliderThatWasMoved == dial.get())
    {
        //[UserSliderCode_dial] -- add your slider handling code here..
        //[/UserSliderCode_dial]
    }
    else if (sliderThatWasMoved == sliderMasterVolume.get())
    {
        //[UserSliderCode_sliderMasterVolume] -- add your slider handling code here..
        //[/UserSliderCode_sliderMasterVolume]
    }
    else if (sliderThatWasMoved == sliderAudioInVolume.get())
    {
        //[UserSliderCode_sliderAudioInVolume] -- add your slider handling code here..
        //[/UserSliderCode_sliderAudioInVolume]
    }

    //[UsersliderValueChanged_Post]
    //[/UsersliderValueChanged_Post]
}

void MainComponent::buttonClicked (juce::Button* buttonThatWasClicked)
{
    //[UserbuttonClicked_Pre]
    //[/UserbuttonClicked_Pre]

    if (buttonThatWasClicked == buttonPlay.get())
    {
        //[UserButtonCode_buttonPlay] -- add your button handler code here..
        //[/UserButtonCode_buttonPlay]
    }
    else if (buttonThatWasClicked == buttonEdit.get())
    {
        //[UserButtonCode_buttonEdit] -- add your button handler code here..
        //[/UserButtonCode_buttonEdit]
    }
    else if (buttonThatWasClicked == buttonUtil.get())
    {
        //[UserButtonCode_buttonUtil] -- add your button handler code here..
        //[/UserButtonCode_buttonUtil]
    }
    else if (buttonThatWasClicked == buttonEffect.get())
    {
        //[UserButtonCode_buttonEffect] -- add your button handler code here..
        //[/UserButtonCode_buttonEffect]
    }
    else if (buttonThatWasClicked == buttonMode.get())
    {
        //[UserButtonCode_buttonMode] -- add your button handler code here..
        //[/UserButtonCode_buttonMode]
    }
    else if (buttonThatWasClicked == buttonEQ.get())
    {
        //[UserButtonCode_buttonEQ] -- add your button handler code here..
        //[/UserButtonCode_buttonEQ]
    }
    else if (buttonThatWasClicked == buttonMuteSolo.get())
    {
        //[UserButtonCode_buttonMuteSolo] -- add your button handler code here..
        //[/UserButtonCode_buttonMuteSolo]
    }
    else if (buttonThatWasClicked == buttonPartDec.get())
    {
        //[UserButtonCode_buttonPartDec] -- add your button handler code here..
        //[/UserButtonCode_buttonPartDec]
    }
    else if (buttonThatWasClicked == buttonPartInc.get())
    {
        //[UserButtonCode_buttonPartInc] -- add your button handler code here..
        //[/UserButtonCode_buttonPartInc]
    }
    else if (buttonThatWasClicked == buttonEnter.get())
    {
        //[UserButtonCode_buttonEnter] -- add your button handler code here..
        //[/UserButtonCode_buttonEnter]
    }
    else if (buttonThatWasClicked == buttonSelectDec.get())
    {
        //[UserButtonCode_buttonSelectDec] -- add your button handler code here..
        //[/UserButtonCode_buttonSelectDec]
    }
    else if (buttonThatWasClicked == buttonSelectInc.get())
    {
        //[UserButtonCode_buttonSelectInc] -- add your button handler code here..
        //[/UserButtonCode_buttonSelectInc]
    }
    else if (buttonThatWasClicked == buttonExit.get())
    {
        //[UserButtonCode_buttonExit] -- add your button handler code here..
        //[/UserButtonCode_buttonExit]
    }
    else if (buttonThatWasClicked == buttonValueDec.get())
    {
        //[UserButtonCode_buttonValueDec] -- add your button handler code here..
        //[/UserButtonCode_buttonValueDec]
    }
    else if (buttonThatWasClicked == buttonValueInc.get())
    {
        //[UserButtonCode_buttonValueInc] -- add your button handler code here..
        //[/UserButtonCode_buttonValueInc]
    }
    else if (buttonThatWasClicked == buttonSettings.get())
    {
        //[UserButtonCode_buttonSettings] -- add your button handler code here..
        //[/UserButtonCode_buttonSettings]
    }
    else if (buttonThatWasClicked == buttonAudition.get())
    {
        //[UserButtonCode_buttonAudition] -- add your button handler code here..
        //[/UserButtonCode_buttonAudition]
    }
    else if (buttonThatWasClicked == buttonLoad.get())
    {
        //[UserButtonCode_buttonLoad] -- add your button handler code here..
        //[/UserButtonCode_buttonLoad]
    }
    else if (buttonThatWasClicked == buttonPlayPause.get())
    {
        //[UserButtonCode_buttonPlayPause] -- add your button handler code here..
        //[/UserButtonCode_buttonPlayPause]
    }
    else if (buttonThatWasClicked == buttonStop.get())
    {
        //[UserButtonCode_buttonStop] -- add your button handler code here..
        //[/UserButtonCode_buttonStop]
    }
    else if (buttonThatWasClicked == buttonPrev.get())
    {
        //[UserButtonCode_buttonPrev] -- add your button handler code here..
        //[/UserButtonCode_buttonPrev]
    }
    else if (buttonThatWasClicked == buttonNext.get())
    {
        //[UserButtonCode_buttonNext] -- add your button handler code here..
        //[/UserButtonCode_buttonNext]
    }
    else if (buttonThatWasClicked == buttonList.get())
    {
        //[UserButtonCode_buttonList] -- add your button handler code here..
        //[/UserButtonCode_buttonList]
    }

    //[UserbuttonClicked_Post]
    //[/UserbuttonClicked_Post]
}



//[MiscUserCode] You can add your own definitions of your custom methods or any other code here...
void MainComponent::wireButton (juce::ImageButton& b, int index, const juce::String& name)
{
    b.setComponentID (name);
    b.setButtonText (name);
    b.onStateChange = [this, index, button = &b] {
        if (processor) processor->engine.button (index, button->isDown());
    };
}
void MainComponent::connectToProcessor (SMU2000AudioProcessor& p)
{
    processor = &p;
    volumeAttachment = std::make_unique<juce::SliderParameterAttachment> (*p.outputGain, *sliderMasterVolume);
    inputAttachment = std::make_unique<juce::SliderParameterAttachment> (*p.inputGain, *sliderAudioInVolume);
    updateSequenceControls();
    startTimerHz (40);
}
void MainComponent::setSnapshot (const OriginalPanel::Snapshot& state)
{
    originalPanel.snapshot = state;
    int bit = 0;
    for (auto* button : { buttonPlay.get(), buttonEdit.get(), buttonUtil.get(), buttonEffect.get(), buttonMode.get(), buttonEQ.get() })
    {
        const bool on = (state.leds & (1u << bit)) != 0;
        ++bit;
        auto image = [] (const char* data, int size) { return juce::ImageCache::getFromMemory (data, size); };
        button->setImages (false, true, true,
            on ? image (BinaryData::LedButtonOn_Normal_png, BinaryData::LedButtonOn_Normal_pngSize) : image (BinaryData::LedButtonOff_Normal_png, BinaryData::LedButtonOff_Normal_pngSize), 1, juce::Colours::transparentBlack,
            on ? image (BinaryData::LedButtonOn_Over_png, BinaryData::LedButtonOn_Over_pngSize) : image (BinaryData::LedButtonOff_Over_png, BinaryData::LedButtonOff_Over_pngSize), 1, juce::Colours::transparentBlack,
            on ? image (BinaryData::LedButtonOn_Down_png, BinaryData::LedButtonOn_Down_pngSize) : image (BinaryData::LedButtonOff_Down_png, BinaryData::LedButtonOff_Down_pngSize), 1, juce::Colours::transparentBlack);
    }
    repaint (lcd->getBounds());
}
void MainComponent::timerCallback()
{
    if (!processor) return;
   #if ! (JUCE_IOS || JUCE_ANDROID)
    // 開いたままステートを読み込まれたら、窓もその大きさへ
    if (auto* editor = findParentComponentOfClass<juce::AudioProcessorEditor>())
    {
        const int width = processor->editorWidth.load();
        if (editor->getWidth() != width)
            editor->setSize (width, juce::roundToInt (width * 96.0 / 1024.0));
    }
   #endif
    labelCpu->setText(juce::String(processor->getCpuLoadPercent(),1)+"%",juce::dontSendNotification);
    PanelSnapshot state;
    if (processor->engine.readPanel (state)) setSnapshot (state);
    updateSequenceControls();
    if(processor->sequenceError.isNotEmpty()) {
        auto error=processor->sequenceError; processor->sequenceError.clear();
        showSequenceError(error);
    }
}
void MainComponent::showSequenceError(const juce::String& error)
{
    if(error.isNotEmpty()) juce::AlertWindow::showAsync(juce::MessageBoxOptions::makeOptionsOk(
        juce::MessageBoxIconType::WarningIcon,"MIDI Player",error),nullptr);
}
void MainComponent::loadSequences(const juce::Array<juce::File>& files)
{
    if(!processor || files.isEmpty()) return;
    juce::String error;
    if(!processor->loadSequenceFiles(files,error)) showSequenceError(error);
    updateSequenceControls();
}
void MainComponent::chooseSequences()
{
    if(sequenceChooser) return;
    sequenceChooser=std::make_unique<juce::FileChooser>("Load MIDI / RCP / R36 / MDF",juce::File(),"*.mid;*.midi;*.smf;*.rcp;*.r36;*.mdf");
    sequenceChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
        | juce::FileBrowserComponent::canSelectMultipleItems,
        [safe=juce::Component::SafePointer<MainComponent>(this)](const juce::FileChooser& chooser) {
            if(!safe) return;
           #if JUCE_IOS
            // 選んだファイルは許可の付いた URL でしか読めないので、アプリの中へコピーして読む
            juce::Array<juce::File> files;
            const auto songs=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("Songs");
            songs.createDirectory();
            for(const auto& url : chooser.getURLResults()) {
                const auto target=songs.getChildFile(juce::URL::removeEscapeChars(url.getFileName()));
                target.deleteFile();
                juce::FileOutputStream out(target);
                auto in=url.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress));
                if(in==nullptr || !out.openedOk() || out.writeFromInputStream(*in,-1)<0) {
                    safe->showSequenceError("Cannot read "+target.getFileName());
                    continue;
                }
                out.flush();
                files.add(target);
            }
            safe->loadSequences(files);
           #else
            safe->loadSequences(chooser.getResults());
           #endif
            safe->sequenceChooser.reset();
        });
}
#if JUCE_IOS
void MainComponent::chooseRoms()
{
    if(sequenceChooser) return;
    sequenceChooser=std::make_unique<juce::FileChooser>("Open the ROM folder");
    sequenceChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [safe=juce::Component::SafePointer<MainComponent>(this)](const juce::FileChooser& chooser) {
            if(!safe) return;
            const auto folders=chooser.getURLResults();
            safe->sequenceChooser.reset();
            if(folders.isEmpty() || !safe->processor) return;
            if(safe->audioSettingsScreen) safe->audioSettingsScreen->setStatus("Copying ROMs. iCloud files are downloaded first...");
            juce::Thread::launch([safe,folder=folders.getFirst()] {
                juce::String error;
                const bool copied=SynthEngine::copyRomFolder(folder,error);
                juce::MessageManager::callAsync([safe,copied,error]() mutable {
                    if(safe==nullptr || !safe->processor) return;
                    auto& engine=safe->processor->engine;
                    if(!copied || !engine.reloadRoms(error))
                        juce::AlertWindow::showAsync(juce::MessageBoxOptions::makeOptionsOk(juce::MessageBoxIconType::WarningIcon,"ROM",error),nullptr);
                    if(safe->audioSettingsScreen) safe->audioSettingsScreen->setStatus(engine.status());
                });
            });
        });
}
#endif
void MainComponent::stepSequence(int direction)
{
    if(!processor) return;
    juce::String error;
    const auto target = direction<0 && processor->engine.sequence().seconds()>2.0
        ? processor->sequenceIndex : processor->sequenceIndex+direction;
    if(!processor->selectSequence(target,processor->engine.sequence().isPlaying(),error))
        showSequenceError(error);
    updateSequenceControls();
}
void MainComponent::showSequenceList()
{
    if(!processor) return;
    juce::PopupMenu menu;
    for(int i=0;i<processor->sequenceFiles.size();++i)
        menu.addItem(i+1,processor->sequenceFiles[i].getFileName(),true,i==processor->sequenceIndex);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(buttonList.get()),
        [safe=juce::Component::SafePointer<MainComponent>(this)](int result) {
            if(!safe || !safe->processor || result<=0) return;
            juce::String error;
            auto& p=*safe->processor;
            if(!p.selectSequence(result-1,p.engine.sequence().isPlaying(),error)) showSequenceError(error);
            safe->updateSequenceControls();
        });
}
void MainComponent::updateSequenceControls()
{
    const bool loaded=processor && processor->engine.sequence().hasFile();
    buttonLoad->setEnabled(processor!=nullptr);
    buttonPlayPause->setEnabled(loaded); buttonStop->setEnabled(loaded);
    buttonPlayPause->setButtonText(loaded && processor->engine.sequence().isPlaying()?"PAUSE":"PLAY");
    buttonPrev->setEnabled(processor && (processor->sequenceIndex>0
        || (loaded && processor->engine.sequence().seconds()>2.0)));
    buttonNext->setEnabled(processor && processor->sequenceIndex>=0 && processor->sequenceIndex+1<processor->sequenceFiles.size());
    buttonList->setEnabled(processor && !processor->sequenceFiles.isEmpty());
    buttonList->setButtonText(loaded?processor->sequenceFileName:"No song");
}
bool MainComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    if(files.isEmpty()) return false;
    for(const auto& file:files) if(!juce::File(file).hasFileExtension("mid;midi;smf;rcp;r36;mdf")) return false;
    return true;
}
void MainComponent::filesDropped(const juce::StringArray& paths,int,int)
{
    if(!isInterestedInFileDrag(paths)) return;
    juce::Array<juce::File> files;
    for(const auto& path:paths) files.add(juce::File(path));
    loadSequences(files);
}
//[/MiscUserCode]


//==============================================================================
#if 0
/*  -- Projucer information section --

    This is where the Projucer stores the metadata that describe this GUI layout, so
    make changes in here at your peril!

BEGIN_JUCER_METADATA

<JUCER_COMPONENT documentType="Component" className="MainComponent" componentName=""
                 parentClasses="public juce::Component, private juce::Timer, public juce::FileDragAndDropTarget"
                 constructorParams="" variableInitialisers="" snapPixels="8" snapActive="1"
                 snapShown="1" overlayOpacity="0.330" fixedSize="1" initialWidth="1024"
                 initialHeight="92">
  <BACKGROUND backgroundColour="ff323e44">
    <IMAGE pos="0 0 1024 92" resource="BinaryData::TX2000W_png" opacity="1.0"
           mode="0"/>
  </BACKGROUND>
  <GENERICCOMPONENT name="" id="467e9c0caff453fc" memberName="lcd" virtualName=""
                    explicitFocusOrder="0" pos="350 18 187 59" class="juce::Component"
                    params=""/>
  <SLIDER name="" id="f45dcbb1a7ad64c" memberName="dial" virtualName="PanelEncoder"
          explicitFocusOrder="0" pos="748 12 72 72" min="0.0" max="10.0"
          int="0.0" style="RotaryVerticalDrag" textBoxPos="NoTextBox" textBoxEditable="1"
          textBoxWidth="80" textBoxHeight="20" skewFactor="1.0" needsCallback="1"
          filmstripImage="BinaryData::Dial_png" filmstripFrames="24" filmstripVertical="1"/>
  <IMAGEBUTTON name="" id="ea081724936000cc" memberName="buttonPlay" virtualName=""
               explicitFocusOrder="0" pos="616 22 13 13" buttonText="new button"
               connectedEdges="0" needsCallback="1" radioGroupId="0" keepProportions="1"
               resourceNormal="BinaryData::LedButtonOff_Normal_png" opacityNormal="1.0"
               colourNormal="0" resourceOver="BinaryData::LedButtonOff_Over_png"
               opacityOver="1.0" colourOver="0" resourceDown="BinaryData::LedButtonOff_Down_png"
               opacityDown="1.0" colourDown="0"/>
  <IMAGEBUTTON name="" id="576a759936a3a9d4" memberName="buttonEdit" virtualName=""
               explicitFocusOrder="0" pos="636 22 13 13" buttonText="new button"
               connectedEdges="0" needsCallback="1" radioGroupId="0" keepProportions="1"
               resourceNormal="BinaryData::LedButtonOff_Normal_png" opacityNormal="1.0"
               colourNormal="0" resourceOver="BinaryData::LedButtonOff_Over_png"
               opacityOver="1.0" colourOver="0" resourceDown="BinaryData::LedButtonOff_Down_png"
               opacityDown="1.0" colourDown="0"/>
  <IMAGEBUTTON name="" id="389e5848dda12712" memberName="buttonUtil" virtualName=""
               explicitFocusOrder="0" pos="616 44 13 13" buttonText="new button"
               connectedEdges="0" needsCallback="1" radioGroupId="0" keepProportions="1"
               resourceNormal="BinaryData::LedButtonOff_Normal_png" opacityNormal="1.0"
               colourNormal="0" resourceOver="BinaryData::LedButtonOff_Over_png"
               opacityOver="1.0" colourOver="0" resourceDown="BinaryData::LedButtonOff_Down_png"
               opacityDown="1.0" colourDown="0"/>
  <IMAGEBUTTON name="" id="2d38f3854ea78976" memberName="buttonEffect" virtualName=""
               explicitFocusOrder="0" pos="636 44 13 13" buttonText="new button"
               connectedEdges="0" needsCallback="1" radioGroupId="0" keepProportions="1"
               resourceNormal="BinaryData::LedButtonOff_Normal_png" opacityNormal="1.0"
               colourNormal="0" resourceOver="BinaryData::LedButtonOff_Over_png"
               opacityOver="1.0" colourOver="0" resourceDown="BinaryData::LedButtonOff_Down_png"
               opacityDown="1.0" colourDown="0"/>
  <IMAGEBUTTON name="" id="52dd80c270721d18" memberName="buttonMode" virtualName=""
               explicitFocusOrder="0" pos="616 66 13 13" buttonText="new button"
               connectedEdges="0" needsCallback="1" radioGroupId="0" keepProportions="1"
               resourceNormal="BinaryData::LedButtonOff_Normal_png" opacityNormal="1.0"
               colourNormal="0" resourceOver="BinaryData::LedButtonOff_Over_png"
               opacityOver="1.0" colourOver="0" resourceDown="BinaryData::LedButtonOff_Down_png"
               opacityDown="1.0" colourDown="0"/>
  <IMAGEBUTTON name="" id="4efaa13474f56e28" memberName="buttonEQ" virtualName=""
               explicitFocusOrder="0" pos="636 66 13 13" buttonText="new button"
               connectedEdges="0" needsCallback="1" radioGroupId="0" keepProportions="1"
               resourceNormal="BinaryData::LedButtonOff_Normal_png" opacityNormal="1.0"
               colourNormal="0" resourceOver="BinaryData::LedButtonOff_Over_png"
               opacityOver="1.0" colourOver="0" resourceDown="BinaryData::LedButtonOff_Down_png"
               opacityDown="1.0" colourDown="0"/>
  <IMAGEBUTTON name="" id="32abf0ec344ef04e" memberName="buttonMuteSolo" virtualName=""
               explicitFocusOrder="0" pos="668 21 16 16" buttonText="new button"
               connectedEdges="0" needsCallback="1" radioGroupId="0" keepProportions="1"
               resourceNormal="BinaryData::Button_Down_png" opacityNormal="1.0"
               colourNormal="0" resourceOver="BinaryData::Button_Over_png" opacityOver="1.0"
               colourOver="0" resourceDown="BinaryData::Button_Down_png" opacityDown="1.0"
               colourDown="0"/>
  <IMAGEBUTTON name="" id="a7c3e622627fb8f0" memberName="buttonPartDec" virtualName=""
               explicitFocusOrder="0" pos="689 21 16 16" buttonText="new button"
               connectedEdges="0" needsCallback="1" radioGroupId="0" keepProportions="1"
               resourceNormal="BinaryData::Button_Down_png" opacityNormal="1.0"
               colourNormal="0" resourceOver="BinaryData::Button_Over_png" opacityOver="1.0"
               colourOver="0" resourceDown="BinaryData::Button_Down_png" opacityDown="1.0"
               colourDown="0"/>
  <IMAGEBUTTON name="" id="53e9459e3b7f3efc" memberName="buttonPartInc" virtualName=""
               explicitFocusOrder="0" pos="710 21 16 16" buttonText="new button"
               connectedEdges="0" needsCallback="1" radioGroupId="0" keepProportions="1"
               resourceNormal="BinaryData::Button_Down_png" opacityNormal="1.0"
               colourNormal="0" resourceOver="BinaryData::Button_Over_png" opacityOver="1.0"
               colourOver="0" resourceDown="BinaryData::Button_Down_png" opacityDown="1.0"
               colourDown="0"/>
  <IMAGEBUTTON name="" id="64a371413008ac8d" memberName="buttonEnter" virtualName=""
               explicitFocusOrder="0" pos="668 43 16 16" buttonText="new button"
               connectedEdges="0" needsCallback="1" radioGroupId="0" keepProportions="1"
               resourceNormal="BinaryData::Button_Down_png" opacityNormal="1.0"
               colourNormal="0" resourceOver="BinaryData::Button_Over_png" opacityOver="1.0"
               colourOver="0" resourceDown="BinaryData::Button_Down_png" opacityDown="1.0"
               colourDown="0"/>
  <IMAGEBUTTON name="" id="3e63747eb98cd32c" memberName="buttonSelectDec" virtualName=""
               explicitFocusOrder="0" pos="689 43 16 16" buttonText="new button"
               connectedEdges="0" needsCallback="1" radioGroupId="0" keepProportions="1"
               resourceNormal="BinaryData::Button_Down_png" opacityNormal="1.0"
               colourNormal="0" resourceOver="BinaryData::Button_Over_png" opacityOver="1.0"
               colourOver="0" resourceDown="BinaryData::Button_Down_png" opacityDown="1.0"
               colourDown="0"/>
  <IMAGEBUTTON name="" id="c5ffcb912e8c1f7d" memberName="buttonSelectInc" virtualName=""
               explicitFocusOrder="0" pos="710 43 16 16" buttonText="new button"
               connectedEdges="0" needsCallback="1" radioGroupId="0" keepProportions="1"
               resourceNormal="BinaryData::Button_Down_png" opacityNormal="1.0"
               colourNormal="0" resourceOver="BinaryData::Button_Over_png" opacityOver="1.0"
               colourOver="0" resourceDown="BinaryData::Button_Down_png" opacityDown="1.0"
               colourDown="0"/>
  <IMAGEBUTTON name="" id="bbb2839040157968" memberName="buttonExit" virtualName=""
               explicitFocusOrder="0" pos="668 65 16 16" buttonText="new button"
               connectedEdges="0" needsCallback="1" radioGroupId="0" keepProportions="1"
               resourceNormal="BinaryData::Button_Down_png" opacityNormal="1.0"
               colourNormal="0" resourceOver="BinaryData::Button_Over_png" opacityOver="1.0"
               colourOver="0" resourceDown="BinaryData::Button_Down_png" opacityDown="1.0"
               colourDown="0"/>
  <IMAGEBUTTON name="" id="431d2462641e98ac" memberName="buttonValueDec" virtualName=""
               explicitFocusOrder="0" pos="689 65 16 16" buttonText="new button"
               connectedEdges="0" needsCallback="1" radioGroupId="0" keepProportions="1"
               resourceNormal="BinaryData::Button_Down_png" opacityNormal="1.0"
               colourNormal="0" resourceOver="BinaryData::Button_Over_png" opacityOver="1.0"
               colourOver="0" resourceDown="BinaryData::Button_Down_png" opacityDown="1.0"
               colourDown="0"/>
  <IMAGEBUTTON name="" id="7e92c530f015635d" memberName="buttonValueInc" virtualName=""
               explicitFocusOrder="0" pos="710 65 16 16" buttonText="new button"
               connectedEdges="0" needsCallback="1" radioGroupId="0" keepProportions="1"
               resourceNormal="BinaryData::Button_Down_png" opacityNormal="1.0"
               colourNormal="0" resourceOver="BinaryData::Button_Over_png" opacityOver="1.0"
               colourOver="0" resourceDown="BinaryData::Button_Down_png" opacityDown="1.0"
               colourDown="0"/>
  <SLIDER name="" id="1c2c78ddda29d7d9" memberName="sliderMasterVolume"
          virtualName="" explicitFocusOrder="0" pos="153 31 24 24" min="0.0"
          max="10.0" int="0.0" style="RotaryVerticalDrag" textBoxPos="NoTextBox"
          textBoxEditable="1" textBoxWidth="80" textBoxHeight="20" skewFactor="1.0"
          needsCallback="1" filmstripImage="BinaryData::Knob_png" filmstripFrames="101"
          filmstripVertical="1"/>
  <SLIDER name="" id="b2803c5f5701543d" memberName="sliderAudioInVolume"
          virtualName="" explicitFocusOrder="0" pos="285 31 24 24" min="0.0"
          max="10.0" int="0.0" style="RotaryVerticalDrag" textBoxPos="NoTextBox"
          textBoxEditable="1" textBoxWidth="80" textBoxHeight="20" skewFactor="1.0"
          needsCallback="1" filmstripImage="BinaryData::Knob_png" filmstripFrames="101"
          filmstripVertical="1"/>
  <IMAGEBUTTON name="" id="ff30971d4d2497ec" memberName="buttonSettings" virtualName=""
               explicitFocusOrder="0" pos="890 28 36 36" buttonText="" connectedEdges="0"
               needsCallback="1" radioGroupId="0" keepProportions="1" resourceNormal="BinaryData::Power_normal_png"
               opacityNormal="1.0" colourNormal="0" resourceOver="BinaryData::Power_over_png"
               opacityOver="1.0" colourOver="0" resourceDown="BinaryData::Power_down_png"
               opacityDown="1.0" colourDown="0"/>
  <IMAGEBUTTON name="" id="a7c31f5e92d04b68" memberName="buttonAudition" virtualName=""
               explicitFocusOrder="0" pos="108 31 28 28" buttonText="" connectedEdges="0"
               needsCallback="1" radioGroupId="0" keepProportions="1" resourceNormal=""
               opacityNormal="1.0" colourNormal="0" resourceOver="" opacityOver="1.0"
               colourOver="0" resourceDown="" opacityDown="1.0" colourDown="0"/>
  <TEXTBUTTON name="" id="63be284b314fb4d1" memberName="buttonLoad" virtualName=""
              explicitFocusOrder="0" pos="128 74 40 16" bgColOff="ff121212"
              textCol="ffffffff" buttonText="LOAD" connectedEdges="15" needsCallback="1"
              radioGroupId="0"/>
  <TEXTBUTTON name="" id="d87ef06f4e7c7b7d" memberName="buttonPlayPause" virtualName=""
              explicitFocusOrder="0" pos="84 74 40 16" bgColOff="ff121212"
              buttonText="PLAY" connectedEdges="15" needsCallback="1" radioGroupId="0"/>
  <TEXTBUTTON name="" id="962109b7c913bbd4" memberName="buttonStop" virtualName=""
              explicitFocusOrder="0" pos="40 74 40 16" bgColOff="ff121212"
              buttonText="STOP" connectedEdges="15" needsCallback="1" radioGroupId="0"/>
  <TEXTBUTTON name="" id="7147d2ebb9a6760e" memberName="buttonPrev" virtualName=""
              explicitFocusOrder="0" pos="172 74 16 16" bgColOff="ff121212"
              textCol="ffffffff" buttonText="|&lt;" connectedEdges="15" needsCallback="1"
              radioGroupId="0"/>
  <TEXTBUTTON name="" id="32c51b74eeee5075" memberName="buttonNext" virtualName=""
              explicitFocusOrder="0" pos="308 74 16 16" bgColOff="ff121212"
              textCol="ffffffff" buttonText="&gt;|" connectedEdges="15" needsCallback="1"
              radioGroupId="0"/>
  <TEXTBUTTON name="" id="214df8044433499c" memberName="buttonList" virtualName=""
              explicitFocusOrder="0" pos="192 74 112 16" bgColOff="ff121212"
              textCol="ffffffff" buttonText="" connectedEdges="15" needsCallback="1"
              radioGroupId="0"/>
  <LABEL name="" id="675d0d5faf68fc3d" memberName="labelCpu" virtualName=""
         explicitFocusOrder="0" pos="960 32 40 24" textCol="80ffffff"
         edTextCol="ff000000" edBkgCol="0" labelText="0%" editableSingleClick="0"
         editableDoubleClick="0" focusDiscardsChanges="0" fontname="Default font"
         fontsize="15.0" kerning="0.0" bold="0" italic="0" justification="34"/>
</JUCER_COMPONENT>

END_JUCER_METADATA
*/
#endif


//[EndFile] You can add extra defines here...
//[/EndFile]

