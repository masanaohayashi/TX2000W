#include "../Source/PluginEditor.h"
#include <iostream>

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    for (auto type : {juce::AudioProcessor::wrapperType_Standalone,
                      juce::AudioProcessor::wrapperType_AudioUnitv3,
                      juce::AudioProcessor::wrapperType_VST3})
    {
        juce::AudioProcessor::setTypeOfNextNewPlugin(type);
        SMU2000AudioProcessor p;
        SMU2000AudioProcessorEditor editor(p);
        if (!editor.isResizable() || editor.getWidth()!=1024 || editor.getHeight()!=96) return 1;
        MainComponent* panel=nullptr;
        int panels=0;
        for (auto* child : editor.getChildren())
            if (auto* main=dynamic_cast<MainComponent*>(child)) { panel=main; ++panels; }
        if (panels!=1) return 2;
        for (const int width : {1024,1280,1536,2048,4096})
        {
            const int height=juce::roundToInt(width*96.0/1024.0);
            editor.setBoundsConstrained({0,0,width,height});
            if (editor.getWidth()!=width || editor.getHeight()!=height) return 3;
            const auto area=editor.getLocalArea(panel,panel->getLocalBounds());
            if (area!=juce::Rectangle<int>(0,0,width,juce::roundToInt(height*92.0/96.0))) return 4;
        }
        editor.setBoundsConstrained({0,0,300,100});
        if (editor.getWidth()<1024 || editor.getHeight()<96) return 5;
        editor.setBoundsConstrained({0,0,5000,3000});
        if (editor.getWidth()>4096 || editor.getHeight()>384) return 6;
        // 大きさはステートに残り、次に開くときも、開いたまま読み込むときも戻る
        editor.setSize(2048,192);
        juce::MemoryBlock sized;
        p.getStateInformation(sized);
        {
            SMU2000AudioProcessor reopened;
            reopened.setStateInformation(sized.getData(),int(sized.getSize()));
            SMU2000AudioProcessorEditor again(reopened);
            if (again.getWidth()!=2048 || again.getHeight()!=192) return 30;
        }
        editor.setSize(1024,96);
        p.setStateInformation(sized.getData(),int(sized.getSize()));
        panel->timerCallback();
        if (editor.getWidth()!=2048 || editor.getHeight()!=192) return 31;
        // iOS の全面のオーディオ設定: Viewport は中央、閉じるボタンはその外で、押すと閉じる
        {
            juce::AudioDeviceManager manager;
            juce::Value mute;
            int closed=0;
            AudioSettingsScreen screen(manager,2,2,false,&mute,[&]{++closed;});
            screen.setSize(1366,1024);
            auto* view=dynamic_cast<juce::Viewport*>(screen.findChildWithID("audioSettingsViewport"));
            auto* close=dynamic_cast<juce::Button*>(screen.findChildWithID("closeAudioSettings"));
            if (view==nullptr || close==nullptr) return 33;
            const auto shown=view->getBounds();
            if (shown.getCentreX()!=683 || shown.getWidth()>640 || shown.intersects(close->getBounds())
                || !screen.getLocalBounds().contains(close->getBounds()) || view->getViewedComponent()->getWidth()!=view->getMaximumVisibleWidth()) return 34;
            screen.setSize(375,300); // 小さな画面は Viewport でスクロールする
            if (view->getViewedComponent()->getWidth()>view->getWidth()) return 35;
            close->onClick();
            if (closed!=1) return 36;
        }
        // iOS のように窓の比率が違っても、パネルは幅に収めて中央に置く
        editor.setSize(1024,96);
        editor.getConstrainer()->setFixedAspectRatio(0);
        editor.setResizeLimits(1,1,16384,16384);
        for (const auto [width,height] : {std::pair<int,int>{1366,974},{1024,1316},{2000,50}})
        {
            editor.setSize(width,height);
            const auto scale=juce::jmin(width/1024.0f,height/96.0f);
            const auto shown=editor.getLocalArea(panel,panel->getLocalBounds().toFloat());
            if (std::abs(shown.getCentreX()-width/2.0f)>1 || std::abs(shown.getY()-(height-96*scale)/2)>1 || std::abs(shown.getWidth()-1024*scale)>1) return 32;
        }
        editor.setSize(1024,96);
        editor.setResizeLimits(SMU2000AudioProcessor::MIN_EDITOR_WIDTH,96,SMU2000AudioProcessor::MAX_EDITOR_WIDTH,384);
        editor.getConstrainer()->setFixedAspectRatio(1024.0/96.0);
        p.editorWidth=1024;
        editor.setSize(1024,96);
        auto* master=dynamic_cast<juce::Slider*>(panel->findChildWithID("masterVolume"));
        auto* input=dynamic_cast<juce::Slider*>(panel->findChildWithID("audioInputVolume"));
        if (!master || !input) return 7;
        master->setValue(0.37,juce::sendNotificationSync);
        input->setValue(0.61,juce::sendNotificationSync);
        if (std::abs(p.outputGain->get()-0.37f)>1e-6f || std::abs(p.inputGain->get()-0.61f)>1e-6f) return 8;
        p.outputGain->setValueNotifyingHost(0.83f);
        if (std::abs(master->getValue()-0.83)>1e-6) return 9;
        auto* encoder=dynamic_cast<PanelEncoder*>(panel->findChildWithID("dial"));
        int steps=0; encoder->onStep=[&](int n){steps+=n;};
        encoder->advance(57); encoder->advance(-81);
        if (steps!=-24 || encoder->getValue()!=0) return 10;
        PanelSnapshot lights; lights.leds=8;
        panel->setSnapshot(lights);
        auto* play=dynamic_cast<juce::ImageButton*>(panel->findChildWithID("PLAY"));
        auto* effect=dynamic_cast<juce::ImageButton*>(panel->findChildWithID("EFFECT"));
        if(play->getNormalImage()==effect->getNormalImage()) return 19;
        // MODE and EQ open the sound module and master EQ screens.
        if(panel->findChildWithID("MODE")->isEnabled()!=play->isEnabled() || panel->findChildWithID("EQ")->isEnabled()!=play->isEnabled()) return 20;
        auto* settingsButton=dynamic_cast<juce::Button*>(panel->findChildWithID("settings"));
        auto* settings=panel->findChildWithID("settingsPanel");
        settingsButton->onClick();
        if (!settings->isVisible()) return 11;
        dynamic_cast<SettingsComponent*>(settings)->onClose();
        if (settings->isVisible()) return 12;
        for (const auto* id : {"PLAY","EDIT","UTIL","EFFECT","MUTE / SOLO","PART -","PART +","ENTER","SELECT -","SELECT +","EXIT","VALUE -","VALUE +"})
        {
            auto* button=dynamic_cast<juce::Button*>(panel->findChildWithID(id));
            if (!button || !button->onStateChange) return 13;
        }
        if (type==juce::AudioProcessor::wrapperType_Standalone)
        {
            p.prepareToPlay(44100,256);
            juce::AudioBuffer<float> buffer(2,256); juce::MidiBuffer midi;
            auto render=[&] { for(int n=0;n<100;++n) { buffer.clear(); p.processBlock(buffer,midi); } };
            auto* edit=dynamic_cast<juce::Button*>(panel->findChildWithID("EDIT"));
            edit->setState(juce::Button::buttonDown); render();
            edit->setState(juce::Button::buttonNormal); render();
            PanelSnapshot state; p.engine.readPanel(state); render();
            if (!p.engine.readPanel(state) || !state.lcdOn) return 14;
            auto press=[&](const char* id) {
                auto* button=dynamic_cast<juce::Button*>(panel->findChildWithID(id));
                button->setState(juce::Button::buttonDown); render();
                button->setState(juce::Button::buttonNormal); render();
            };
            press("EFFECT");
            p.engine.readPanel(state); render();
            if (!p.engine.readPanel(state) || !(state.leds & 8)) return 16;
            press("EXIT");
            p.engine.readPanel(state); render();
            if (!p.engine.readPanel(state) || !(state.leds & 1)) return 17;
            render();
            // AUDITION: 絵の PHONES の所にある透明なボタン。押すと鳴る
            auto* audition=dynamic_cast<juce::ImageButton*>(panel->findChildWithID("AUDITION"));
            if (!audition || audition->getBounds()!=juce::Rectangle<int>(108,31,28,28) || panel->getComponentAt(122,45)!=audition || !audition->getNormalImage().isNull()) return 21;
            const auto peakWhile=[&](juce::Button::ButtonState pressed) {
                audition->setState(pressed);
                float peak=0;
                for(int n=0;n<40;++n) { buffer.clear(); p.processBlock(buffer,midi); peak=std::max(peak,buffer.getMagnitude(0,256)); }
                return peak;
            };
            for(int n=0;n<400;++n) { buffer.clear(); p.processBlock(buffer,midi); }
            if (peakWhile(juce::Button::buttonNormal)>1.0e-4f) return 22;
            if (peakWhile(juce::Button::buttonDown)<1.0e-3f) return 23;
            audition->setState(juce::Button::buttonNormal);
            panel->timerCallback();
            auto image=editor.createComponentSnapshot(editor.getLocalBounds(),true,2.0f);
            juce::FileOutputStream output{juce::File(argc>1?argv[1]:"/tmp/smu-rack-ui.png")};
            output.setPosition(0); output.truncate();
            if (!output.openedOk() || !juce::PNGImageFormat().writeImageToStream(image,output)) return 15;
        }
    }
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Undefined);
    std::cout<<"PASS: single rack panel, resizing, parameter bindings, encoder wrap, settings and live LCD"<<std::endl;
}
