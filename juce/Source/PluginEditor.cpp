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
//[/Headers]

#include "PluginEditor.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
SMU2000AudioProcessorEditor::SMU2000AudioProcessorEditor (SMU2000AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    //[Constructor_pre] You can add your own custom stuff here..
    [[maybe_unused]] const int savedWidth = p.editorWidth.load(); // 下の setResizeLimits と setSize で上書きされる前に読む
    //[/Constructor_pre]

    mainComponent.reset (new MainComponent());
    addAndMakeVisible (mainComponent.get());
    mainComponent->setBounds (0, 0, 1024, 92);

    cachedImage_BinaryData_Rack_png_1 = juce::ImageCache::getFromMemory (BinaryData::Rack_png, BinaryData::Rack_pngSize);

    //[UserPreSize]
    setResizable (true, p.wrapperType == juce::AudioProcessor::wrapperType_VST3);
   #if JUCE_IOS || JUCE_ANDROID
    // 窓の大きさは OS が決める。パネルの比率は resized で保つ
    setResizeLimits (1, 1, 16384, 16384);
   #else
    setResizeLimits (SMU2000AudioProcessor::MIN_EDITOR_WIDTH, 96, SMU2000AudioProcessor::MAX_EDITOR_WIDTH, 384);
    getConstrainer()->setFixedAspectRatio (1024.0 / 96.0);
   #endif
    //[/UserPreSize]

    setSize (1024, 96);


    //[Constructor] You can add your own custom stuff here..
   #if ! (JUCE_IOS || JUCE_ANDROID)
    setSize (savedWidth, juce::roundToInt (savedWidth * 96.0 / 1024.0));
   #endif
    static_cast<MainComponent*>(mainComponent.get())->connectToProcessor(p);
    //[/Constructor]
}

SMU2000AudioProcessorEditor::~SMU2000AudioProcessorEditor()
{
    //[Destructor_pre]. You can add your own custom destruction code here..
    //[/Destructor_pre]

    mainComponent = nullptr;


    //[Destructor]. You can add your own custom destruction code here..
    //[/Destructor]
}

//==============================================================================
void SMU2000AudioProcessorEditor::paint (juce::Graphics& g)
{
    //[UserPrePaint] Add your own custom painting code here..
    //[/UserPrePaint]

    g.fillAll (juce::Colours::black);

    {
        int x = 0, y = 0, width = 1024, height = 96;
        //[UserPaintCustomArguments] Customize the painting arguments here..
        width = getWidth(); height = getHeight();
        //[/UserPaintCustomArguments]
        g.setColour (juce::Colours::black);
        g.drawImageWithin (cachedImage_BinaryData_Rack_png_1,
                           x, y, width, height,
                           juce::RectanglePlacement::centred,
                           false);
    }

    //[UserPaint] Add your own custom painting code here..
    //[/UserPaint]
}

void SMU2000AudioProcessorEditor::resized()
{
    //[UserPreResize] Add your own custom resize code here..
    //[/UserPreResize]

    //[UserResized] Add your own custom resize handling here..
    // パネルは比率を保ったまま中央に置く
    const auto scale = juce::jmin (getWidth() / 1024.0f, getHeight() / 96.0f);
    mainComponent->setTransform (juce::AffineTransform::scale (scale)
        .translated ((getWidth() - 1024.0f * scale) / 2.0f, (getHeight() - 96.0f * scale) / 2.0f));
   #if ! (JUCE_IOS || JUCE_ANDROID)
    audioProcessor.editorWidth = getWidth();
   #endif
    //[/UserResized]
}



//[MiscUserCode] You can add your own definitions of your custom methods or any other code here...
//[/MiscUserCode]


//==============================================================================
#if 0
/*  -- Projucer information section --

    This is where the Projucer stores the metadata that describe this GUI layout, so
    make changes in here at your peril!

BEGIN_JUCER_METADATA

<JUCER_COMPONENT documentType="Component" className="SMU2000AudioProcessorEditor"
                 componentName="" parentClasses="public juce::AudioProcessorEditor"
                 constructorParams="SMU2000AudioProcessor&amp; p" variableInitialisers="AudioProcessorEditor (&amp;p), audioProcessor (p)"
                 snapPixels="8" snapActive="1" snapShown="1" overlayOpacity="0.330"
                 fixedSize="1" initialWidth="1024" initialHeight="96">
  <BACKGROUND backgroundColour="ff000000">
    <IMAGE pos="0 0 1024 96" resource="BinaryData::Rack_png" opacity="1.0"
           mode="1"/>
  </BACKGROUND>
  <JUCERCOMP name="" id="ade545e64a05e3a9" memberName="mainComponent" virtualName=""
             explicitFocusOrder="0" pos="0 0 1024 92" sourceFile="MainComponent.cpp"
             constructorParams=""/>
</JUCER_COMPONENT>

END_JUCER_METADATA
*/
#endif


//[EndFile] You can add extra defines here...
//[/EndFile]

