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
#include <JuceHeader.h>
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
    //[/Headers]

#include "SettingsComponent.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
SettingsComponent::SettingsComponent ()
{
    //[Constructor_pre] You can add your own custom stuff here..
    //[/Constructor_pre]

    buttonClose.reset (new juce::TextButton (juce::String()));
    addAndMakeVisible (buttonClose.get());
    buttonClose->setButtonText (TRANS ("x"));
    buttonClose->addListener (this);

    buttonClose->setBounds (8, 8, 24, 24);

    buttonAudioDeviceSettings.reset (new juce::TextButton (juce::String()));
    addAndMakeVisible (buttonAudioDeviceSettings.get());
    buttonAudioDeviceSettings->setButtonText (TRANS ("AUDIO DEVICE SETTINGS..."));
    buttonAudioDeviceSettings->addListener (this);

    buttonAudioDeviceSettings->setBounds (64, 16, 184, 24);


    //[UserPreSize]
    //[/UserPreSize]

    setSize (1024, 92);


    //[Constructor] You can add your own custom stuff here..
    buttonClose->setComponentID ("closeSettings");
    buttonAudioDeviceSettings->setComponentID ("audioDeviceSettings");
    buttonAudioDeviceSettings->setEnabled (juce::StandalonePluginHolder::getInstance() != nullptr);
    //[/Constructor]
}

SettingsComponent::~SettingsComponent()
{
    //[Destructor_pre]. You can add your own custom destruction code here..
    //[/Destructor_pre]

    buttonClose = nullptr;
    buttonAudioDeviceSettings = nullptr;


    //[Destructor]. You can add your own custom destruction code here..
    //[/Destructor]
}

//==============================================================================
void SettingsComponent::paint (juce::Graphics& g)
{
    //[UserPrePaint] Add your own custom painting code here..
    //[/UserPrePaint]

    g.fillAll (juce::Colour (0xff121212));

    //[UserPaint] Add your own custom painting code here..
    //[/UserPaint]
}

void SettingsComponent::resized()
{
    //[UserPreResize] Add your own custom resize code here..
    //[/UserPreResize]

    //[UserResized] Add your own custom resize handling here..
    //[/UserResized]
}

void SettingsComponent::buttonClicked (juce::Button* buttonThatWasClicked)
{
    //[UserbuttonClicked_Pre]
    //[/UserbuttonClicked_Pre]

    if (buttonThatWasClicked == buttonClose.get())
    {
        //[UserButtonCode_buttonClose] -- add your button handler code here..
        if (onClose) onClose();
    //[/UserButtonCode_buttonClose]
    }
    else if (buttonThatWasClicked == buttonAudioDeviceSettings.get())
    {
        //[UserButtonCode_buttonAudioDeviceSettings] -- add your button handler code here..
        if (auto* holder = juce::StandalonePluginHolder::getInstance()) holder->showAudioSettingsDialog();
    //[/UserButtonCode_buttonAudioDeviceSettings]
    }

    //[UserbuttonClicked_Post]
    //[/UserbuttonClicked_Post]
}



//[MiscUserCode] You can add your own definitions of your custom methods or any other code here...
//[/MiscUserCode]


//==============================================================================
#if 0
/*  -- Projucer information section --

    This is where the Projucer stores the metadata that describe this GUI layout, so
    make changes in here at your peril!

BEGIN_JUCER_METADATA

<JUCER_COMPONENT documentType="Component" className="SettingsComponent" componentName=""
                 parentClasses="public juce::Component" constructorParams="" variableInitialisers=""
                 snapPixels="8" snapActive="1" snapShown="1" overlayOpacity="0.330"
                 fixedSize="1" initialWidth="1024" initialHeight="92">
  <BACKGROUND backgroundColour="ff121212"/>
  <TEXTBUTTON name="" id="fac85e47b2ed574f" memberName="buttonClose" virtualName=""
              explicitFocusOrder="0" pos="8 8 24 24" buttonText="x" connectedEdges="0"
              needsCallback="1" radioGroupId="0"/>
  <TEXTBUTTON name="" id="24c70fbf08131d21" memberName="buttonAudioDeviceSettings"
              virtualName="" explicitFocusOrder="0" pos="64 16 184 24" buttonText="AUDIO DEVICE SETTINGS..."
              connectedEdges="0" needsCallback="1" radioGroupId="0"/>
</JUCER_COMPONENT>

END_JUCER_METADATA
*/
#endif


//[EndFile] You can add extra defines here...
//[/EndFile]

