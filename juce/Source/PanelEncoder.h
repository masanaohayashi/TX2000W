#pragma once
#include <JuceHeader.h>
class PanelEncoder : public juce::Slider
{
public:
    using juce::Slider::Slider;
    std::function<void(int)> onStep;
    void mouseDown(const juce::MouseEvent& e) override { lastY=e.position.y; remainder=0; }
    void mouseDrag(const juce::MouseEvent& e) override {
        remainder+=(lastY-e.position.y)/3.0f; lastY=e.position.y;
        const int steps=int(remainder); remainder-=steps; advance(steps);
    }
    void mouseUp(const juce::MouseEvent&) override {}
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails& wheel) override {
        if (wheel.isInertial) return;
        remainder+=(wheel.deltaY!=0?wheel.deltaY:-wheel.deltaX)*24.0f;
        const int steps=int(remainder); remainder-=steps; advance(steps);
    }
    bool keyPressed(const juce::KeyPress& key) override {
        if (key == juce::KeyPress::upKey || key == juce::KeyPress::rightKey) { advance(1); return true; }
        if (key == juce::KeyPress::downKey || key == juce::KeyPress::leftKey) { advance(-1); return true; }
        return false;
    }
    void advance(int steps) {
        if (!steps) return;
        const int frame=((int(getValue())+steps)%24+24)%24;
        setValue(frame,juce::dontSendNotification);
        if(onStep) onStep(steps);
    }
private:
    float lastY=0,remainder=0;
};
