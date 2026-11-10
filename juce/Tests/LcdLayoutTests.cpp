#include "../Source/OriginalPanel.h"
#include <iostream>
#include <cmath>

static juce::Rectangle<float> litBounds(float scale,int column,int line=0,int mask=0x10)
{
    OriginalPanel panel;
    panel.snapshot.lcdOn=true;
    const auto draw=[&]{
        juce::Image raster(juce::Image::ARGB,int(1000*scale),int(400*scale),true);
        juce::Graphics g(raster);
        g.addTransform(juce::AffineTransform::scale(scale));
        panel.drawLCD(g,scale);
        return raster;
    };
    // The PAN arc is always lit, so look only at what this dot changes.
    const auto unlit=draw();
    panel.snapshot.dots[size_t(column*8+line)]=(unsigned char)mask;
    const auto raster=draw();
    juce::Rectangle<int> bounds;
    for(int y=0;y<raster.getHeight();++y)
        for(int x=0;x<raster.getWidth();++x)
            if(unlit.getPixelAt(x,y)!=raster.getPixelAt(x,y))
                bounds=bounds.getUnion({x,y,1,1});
    return bounds.toFloat()/scale;
}
int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    bool pass=true;
    for(int column : {0,9,17,19}) {
        const auto reference=litBounds(1.0f,column);
        for(float scale : {1.25f,1.5f,2.0f}) {
            const auto actual=litBounds(scale,column);
            const auto shift=actual.getCentre().getDistanceFrom(reference.getCentre());
            std::cout<<"column="<<column<<" scale="<<scale<<" logical displacement="<<shift<<"\n";
            pass &= !actual.isEmpty() && shift<=1.0f;
        }
    }
    // 文字の点はどこでも同じ大きさ: 上の文字、PART、MIDI
    const auto same=[&](const char* name,juce::Rectangle<float> a,juce::Rectangle<float> b){
        const bool ok=!a.isEmpty() && std::abs(a.getWidth()-b.getWidth())<=0.5f && std::abs(a.getHeight()-b.getHeight())<=0.5f;
        std::cout<<name<<" "<<a.getWidth()<<"x"<<a.getHeight()<<" / "<<b.getWidth()<<"x"<<b.getHeight()<<"\n";
        pass&=ok;
    };
    constexpr float s=4.0f;
    const auto topDot=litBounds(s,0);
    same("PART dot",topDot,litBounds(s,17));
    same("MIDI dot",topDot,litBounds(s,24+17));
    // 絵の点は幅が文字と同じで、上下の行の間に隙間がある
    const auto iconDot=litBounds(s,20), iconBelow=litBounds(s,20,1);
    pass&=std::abs(iconDot.getWidth()-topDot.getWidth())<=0.5f && iconDot.getBottom()<iconBelow.getY();
    std::cout<<"icon dot width "<<iconDot.getWidth()<<" rows "<<iconDot.getBottom()<<" < "<<iconBelow.getY()<<"\n";
    // VOL と EXP の棒は同じ形（桁 19 の左 2 点と右 2 点）
    same("VOL/EXP",litBounds(s,19,0,0x10),litBounds(s,19,0,0x01));
    // REV・CHO・VAR の一番上の段は同じ形（桁 23 の下 4 ビット）
    const auto fan=litBounds(s,24+23,7,1<<3);
    same("REV/CHO",fan,litBounds(s,24+23,7,1<<2));
    same("REV/VAR",fan,litBounds(s,24+23,7,1<<1));
    std::cout<<(pass?"PASS":"FAIL")<<": LCD elements keep their logical positions across scale factors\n";
    return pass?0:1;
}
