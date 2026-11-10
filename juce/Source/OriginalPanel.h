// BSD-3-Clause. JUCE rendering of art/mame/panel.txt and src/ui/panel.cpp.
// Upstream artwork: hap and Felipe Sanches, CC0-1.0.
#pragma once
#include <JuceHeader.h>
#include "PanelSnapshot.h"
#include "../JuceLibraryCode/BinaryData.h"
#include <map>
#include <array>
#include <sstream>
#include <iomanip>

class PanelImageButton : public juce::ImageButton
{
public:
    juce::String label, sub;
    float panelScale = 1;
    bool keyLegend = false;
    void paintButton(juce::Graphics& g, bool over, bool down) override
    {
        juce::ImageButton::paintButton(g,over,down);
        if(!keyLegend)return;
        g.setColour(juce::Colour(0xff3a3526));
        g.setFont(juce::Font(juce::FontOptions("Segoe UI",8.5f*panelScale,juce::Font::plain)));
        // The key PNG is aspect-fitted with vertical margins. Keep both legend
        // lines inside its face rather than drawing over the top/bottom border.
        g.drawText(label,0,int(5.5f*panelScale),getWidth(),int(10*panelScale),juce::Justification::centred,false);
        g.drawText(sub,0,int(16*panelScale),getWidth(),int(10*panelScale),juce::Justification::centred,false);
    }
};

class OriginalPanel
{
public:
    using Rect = juce::Rectangle<float>;
    using Point = juce::Point<float>;
    using Snapshot=PanelSnapshot;
    Snapshot snapshot;

    struct Button { Rect bounds; juce::String family, label, sub; int led = -1; };
    std::vector<Button> buttons;
    int pressed = -1;
    float volume = 0.72f, wheelAngle = 0;
    juce::String status { juce::String::fromUTF8("音源未接続") };

    OriginalPanel()
    {
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            auto name = juce::String (BinaryData::originalFilenames[i]);
            if (! name.endsWith (".png")) continue;
            int size = 0;
            auto* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size);
            auto im = juce::ImageCache::getFromMemory (data, size);
            jassert (im.isValid());
            images.emplace (name.upToFirstOccurrenceOf ("@2x", false, false), im);
        }
        std::istringstream lines (std::string (BinaryData::panel_txt, BinaryData::panel_txtSize));
        std::string line;
        while (std::getline (lines, line))
        {
            std::istringstream in (line);
            std::string key;
            if (!(in >> key) || key[0] == '#') continue;
            if (key == "text")
            {
                Deco d;
                std::string font, align, colour, text;
                in >> d.x >> d.y >> d.w >> d.h >> font >> align >> colour >> std::quoted (text);
                d.text = juce::String::fromUTF8 (text.c_str());
                d.large = font == "label";
                decorations.push_back (d);
            }
            else
            {
                float n;
                auto& values = layout[juce::String (key)];
                while (in >> n) values.push_back (n);
            }
        }
        // Upstream generated knob coordinates were estimates from text labels.
        // Apply the measured artwork centres, used by rendering AND hit testing.
        std::istringstream corrections(std::string(BinaryData::panelcorrections_txt,
                                                    BinaryData::panelcorrections_txtSize));
        while(std::getline(corrections,line))
        {
            std::istringstream in(line);
            std::string key;
            if(!(in>>key) || key[0]=='#')continue;
            std::vector<float> values;
            float n;
            while(in>>n)values.push_back(n);
            if(values.size()==3)layout[juce::String(key)]=std::move(values);
        }
        const char* modes[] = { "play", "edit", "util", "effect", "sampling", "seq" };
        const char* labels[] = { "PLAY", "EDIT", "UTIL", "EFFECT", "SAMPLING", "SEQ" };
        for (int i = 0; i < 6; ++i)
        {
            auto c = centre (juce::String ("mode.") + modes[i]);
            const auto r = value ("mode.r");
            buttons.push_back ({ { c.x-r, c.y-r, r*2, r*2 }, "btn", labels[i], {}, i });
        }
        const char* nav[] = { "mute_solo", "part-", "part+", "enter", "select-", "select+", "exit", "value-", "value+" };
        const char* names[] = { "MUTE", "PART", "PART", "ENTER", "SELECT", "SELECT", "EXIT", "VALUE", "VALUE" };
        const char* subs[] = { "SOLO", "-", "+", "", "-", "+", "", "-", "+" };
        for (int i = 0; i < 9; ++i)
            buttons.push_back ({ rect (juce::String ("nav.") + nav[i]), "key", names[i], subs[i] });
        const char* cats[] = { "Piano", "Chrom. perc.", "Organ", "Guitar", "Bass", "Strings",
            "Ensemble", "Brass", "Reed", "Pipe", "Synth lead", "Synth pad",
            "Synth effects", "Ethnic", "Percussive", "SFX", "Model excl.", "Drum" };
        for (int i = 0; i < 18; ++i)
            buttons.push_back ({ { value ("cat.x", i%6) - value ("cat.size")/2,
                value ("cat.y", i/6), value ("cat.size"), value ("cat.size", 1) }, "cat", cats[i], {} });
        for (auto name : { "select", "audition" })
        {
            const auto key = juce::String ("round.") + name;
            const auto c = centre (key);
            const auto w = value (key, 2), h = value (key, 3);
            buttons.push_back ({ { c.x-w/2, c.y-h/2, w, h }, "rnd", juce::String (name).toUpperCase(), {} });
        }
    }

    float value (const juce::String& key, size_t i = 0) const
    {
        auto it = layout.find (key);
        jassert (it != layout.end() && i < it->second.size());
        return it != layout.end() && i < it->second.size() ? it->second[i] : 0;
    }
    Point centre (const juce::String& key) const { return { value(key), value(key,1) }; }
    Rect rect (const juce::String& key) const { return {value(key),value(key,1),value(key,2),value(key,3)}; }
    Rect knobBounds (const juce::String& key) const
    {
        auto c = centre(key); auto r = value(key,2);
        return {c.x-r,c.y-r,r*2,r*2};
    }
    int hit (Point p) const
    {
        for (size_t i=0; i<buttons.size(); ++i)
            if (buttons[i].bounds.contains(p)) return (int)i;
        return -1;
    }
    juce::Image getImage (const juce::String& name) const
    {
        auto it=images.find(name);
        jassert(it!=images.end());
        return it!=images.end()?it->second:juce::Image{};
    }

    static void text (juce::Graphics& g, const juce::String& s, Rect r,
                      float size = 8.5f, juce::Colour colour = juce::Colour(0xff2e2c28),
                      juce::Justification align = juce::Justification::centredTop, bool bold = false)
    {
        g.setColour(colour);
        g.setFont(juce::Font(juce::FontOptions("Segoe UI",size,bold ? juce::Font::bold : juce::Font::plain)));
        g.drawText(s,r,align,false);
    }
    void image (juce::Graphics& g, juce::String name, Rect bounds, float degrees = 0) const
    {
        auto it=images.find(name);
        jassert(it!=images.end());
        if (it==images.end()) return;
        juce::Graphics::ScopedSaveState save(g);
        g.addTransform(juce::AffineTransform::rotation(juce::degreesToRadians(degrees),bounds.getCentreX(),bounds.getCentreY()));
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.setColour(juce::Colours::white);
        g.drawImage(it->second,bounds,juce::RectanglePlacement::centred);
    }

    void paint (juce::Graphics& g, float screenScale) const
    {
        g.setColour(juce::Colour(0xffc4bdaa));
        g.fillRect(Rect{0,0,1000,value("body_h")});
        image(g,"mu2000-mame",rect("art"));
        for (const auto& d:decorations)
            text(g,d.text,{d.x,d.y,d.w,d.h},d.large?13.0f:8.5f,
                 juce::Colour(0xff2e2c28),juce::Justification::topLeft,d.large);
        drawLCD(g,screenScale);

        for (size_t i=0;i<buttons.size();++i)
        {
            const auto& b=buttons[i];
            // Interactive images and key legends are owned by ImageButton children.
            if(b.family=="key")
            {
                continue;
            }
            else
            {
                auto c=b.bounds.getCentre();
                const float y=b.family=="btn" ? b.bounds.getY()-19
                            : b.family=="cat" ? b.bounds.getY()-14 : c.y-26;
                const float width=b.family=="rnd"?80.0f:68.0f;
                text(g,b.label,{c.x-width/2,y,width,14});
            }
        }
        const char* plgs[]={"MU","PLG-1","PLG-2","PLG-3"};
        for(int i=0;i<4;++i)
        {
            const float x=value("plg")+i*value("plg",1), y=value("plg",2);
            image(g,(snapshot.leds&(1u<<(6+i)))?"plg-on":"plg",{x-6,y-6,12,12});
            text(g,plgs[i],{x-22,y+7,44,12});
        }
        image(g,"dial",knobBounds("dial"),wheelAngle);
        image(g,"knob",knobBounds("volume"),-135+270*volume);
        auto vc=centre("volume");
        text(g,"VOLUME",{vc.x-36,vc.y+value("volume",2)+4,72,12});
        text(g,status,{20,372,700,13},8.5f,juce::Colour(0xffaaaeb4),juce::Justification::centredLeft);
    }

    void drawLCD (juce::Graphics& g, float scale) const;

private:
    struct Deco { float x=0,y=0,w=0,h=0; bool large=false; juce::String text; };
    std::vector<Deco> decorations;
    std::map<juce::String,std::vector<float>> layout;
    std::map<juce::String,juce::Image> images;
};

// LCD の中の配置は実機の写真から測った。座標は液晶の左上を原点に、幅 836・高さ 265 の単位で持つ。
// 実機の液晶はパネル画像の緑の部分より少し横長なので、点が正方形のままになるよう高さを 96% にして上下に 5 単位ずつ余白を取った。
// 文字の点はどこでも同じ大きさ。絵の点は幅が同じで、縦に 2 倍詰まるぶん高さが低い。VOL と EXP、REV・CHO・VAR はそれぞれ同じ形を並べる。
namespace real_lcd
{
    constexpr float WIDTH=836, HEIGHT=265;
    constexpr float DOT_PITCH=7.625f, DOT_SIZE=6;
    // 上の 2 行 17 桁の文字。1 桁 6 点、1 行 8 点
    constexpr float TEXT_X=27, TEXT_Y=15.6f;
    // 文字の下の目盛りの番号と札。番号は 1 桁に 2 つ
    constexpr float SCALE_FIRST_X=34, SCALE_STEP=3*DOT_PITCH, SCALE_Y=141, SCALE_FONT=14, SCALE_LINE=14;
    constexpr float TAG_Y=154, TAG_HEIGHT=14, TAG_FONT=14, TAG_FIRST=4.8f, TAG_REPEAT=8;
    constexpr float MIC_X=27, MIC_WIDTH=40, MIC_Y=158, LINE_Y=173, MIC_HEIGHT=14.5f, MIC_FONT=12;
    // 下の段: PART の 2 文字、MIDI の 3 文字、絵
    constexpr float LOW_TEXT_Y=190, LOW_TEXT_PITCH=42, PART_X=30, MIDI_X=121;
    // 絵は縦 16 点を文字 8 点分の高さに収める。点の幅は文字と同じで、高さは行の間に隙間が残る分だけ
    constexpr float ICON_X=257, ICON_Y=185, ICON_COLUMN_PITCH=7.34f, ICON_ROW_PITCH=3.74f, ICON_DOT_HEIGHT=2.5f;
    // VOL と EXP: 同じ幅の棒を 8 本
    constexpr float BAR_WIDTH=30, BAR_HEIGHT=4.3f, BAR_PITCH=7.76f, BAR_TOP=188.4f, VOL_CENTER=413, EXP_CENTER=471;
    // PAN
    constexpr float PAN_X=530, PAN_Y=221, PAN_RADIUS=27, PAN_STROKE=2.6f, PAN_ARC=130, PAN_NEEDLE_FROM=8, PAN_NEEDLE_TO=21, PAN_NEEDLE_HALF=PAN_STROKE/2;
    // REV・CHO・VAR: 同じ扇を 3 つ。段は 8
    constexpr float FAN_FIRST_X=590, FAN_STEP=60.5f, FAN_Y=245, FAN_STEP_RADIUS=7.3f, FAN_STROKE=2, FAN_HALF_ANGLE=26;
    // KEY: 符号と 7 セグメント 2 桁
    // 7 セグメントは少し右に傾いている。真ん中の横棒だけ六角形で、ほかは外側が長い台形
    constexpr float KEY_SIGN_X=735, KEY_SIGN_WIDTH=20, KEY_SIGN_HEIGHT=26, KEY_SIGN_GAP=2.2f;
    constexpr float KEY_TEN_X=755, KEY_ONE_X=780, KEY_Y=205.5f, KEY_HEIGHT=40, KEY_DIGIT_WIDTH=20, KEY_SLANT=2.5f, KEY_STROKE=3.2f, KEY_SEGMENT_GAP=1.6f;
    // 右端の矢印は 4 つ。下の 3 つの高さはパネルの XG・GS・PERFORM の字に合わせる
    constexpr float MODE_X=820, MODE_WIDTH=12, MODE_HALF=6;
    constexpr float MODE_Y[4]={181,201,220.5f,240};
    // 項目のカーソル。消えているときも薄く見える
    constexpr float CURSOR_TIP=181.6f, CURSOR_HALF=7, CURSOR_HEIGHT=8;
    constexpr float CURSOR_MIDI=179, CURSOR_BANK=ICON_X+4*ICON_COLUMN_PITCH, CURSOR_PROGRAM=ICON_X+12*ICON_COLUMN_PITCH, CURSOR_KEY=771;
}

// 表示の中身は DDRAM・外字・セグメントのとおり。作り物の文字や目盛りは出さない
inline void OriginalPanel::drawLCD (juce::Graphics& graphics, float scale) const
{
    using namespace real_lcd;
    juce::Graphics::ScopedSaveState save(graphics);
    juce::ignoreUnused(scale);
    auto& g=graphics;
    const auto bounds=rect("lcd");
    // UTIL > SYS の Contrast。値が小さいほど濃い。工場出荷の 2 が基準の色
    const int contrast=juce::jlimit(1,8,int(snapshot.contrast));
    const float ghostAmount=contrast==1 ? 0.35f : 0.06f*float(8-contrast)/6.0f;
    const float inkAmount=contrast==1 ? 1.0f : 1.0f-0.08f*float(contrast-2);
    // 地はパネル画像の黄緑をそのまま見せる。描くのは薄い黒と黒だけ
    const juce::Colour ghost=juce::Colours::black.withAlpha(ghostAmount);
    const juce::Colour ink=juce::Colours::black.withAlpha(inkAmount);
    g.reduceClipRegion(bounds.toNearestInt());
    g.addTransform(juce::AffineTransform::scale(bounds.getWidth()/WIDTH,bounds.getHeight()/HEIGHT).translated(bounds.getX(),bounds.getY()));

    auto bit=[&](int row,int col,int y,int x){return snapshot.lcdOn && (snapshot.dots[size_t((row*24+col)*8+y)]&(1<<x));};
    auto ctl=[&](int col,int row){return bit(row/8,23,row%8,3-col);};
    auto box=[&](bool on,float x,float y,float w,float h){
        g.setColour(on?ink:ghost);g.fillRect(Rect{x,y,w,h});
    };
    auto dot=[&](bool on,float x,float y){box(on,x,y,DOT_SIZE,DOT_SIZE);};
    auto cell=[&](int row,int col,float x,float y){
        for(int yy=0;yy<8;++yy)for(int xx=0;xx<5;++xx)
            dot(bit(row,col,yy,4-xx),x+float(xx)*DOT_PITCH,y+float(yy)*DOT_PITCH);
    };
    auto label=[&](const juce::String& s,float x,float y,float w,float h,float size,bool on){
        text(g,s,{x,y,w,h},size,on?ink:ghost);
    };

    // 上の 2 行
    for(int row=0;row<2;++row)for(int col=0;col<17;++col)
        cell(row,col,TEXT_X+float(col)*6*DOT_PITCH,TEXT_Y+float(row)*8*DOT_PITCH);

    // 目盛りの番号: A1 A2 1-32
    for(int i=0;i<34;++i)
    {
        const float x=SCALE_FIRST_X+float(i)*SCALE_STEP;
        label(i<2?"A"+juce::String(i+1):juce::String(i-1),x-SCALE_STEP/2,SCALE_Y,SCALE_STEP,SCALE_LINE,SCALE_FONT,ctl(3,i<2?3:4));
    }
    // MIC と LINE: 枠の中に字が抜けている部品
    auto tag=[&](const juce::String& s,float y,bool on){
        juce::Path shape;
        shape.addRoundedRectangle(MIC_X,y,MIC_WIDTH,MIC_HEIGHT,2.0f);
        juce::GlyphArrangement glyphs;
        glyphs.addFittedText(juce::Font(juce::FontOptions("Segoe UI",MIC_FONT,juce::Font::plain)),s,MIC_X,y,MIC_WIDTH,MIC_HEIGHT,juce::Justification::centred,1);
        juce::Path letters;
        glyphs.createPath(letters);
        shape.addPath(letters);
        shape.setUsingNonZeroWinding(false);
        g.setColour(on?ink:ghost);
        g.fillPath(shape);
    };
    tag("MIC",MIC_Y,ctl(3,1));
    tag("LINE",LINE_Y,ctl(3,2));
    // BANK と PGM#: 番号 3-4、11-12、19-20、27-28 のあたり
    for(int i=0;i<4;++i)
    {
        const float x=SCALE_FIRST_X+(TAG_FIRST+float(i)*TAG_REPEAT)*SCALE_STEP;
        label(i%2?"PGM#":"BANK",x-30,TAG_Y,60,TAG_HEIGHT,TAG_FONT,ctl(3,i<2?0:5));
    }

    // 下の段の文字
    for(int i=0;i<2;++i)cell(0,17+i,PART_X+float(i)*LOW_TEXT_PITCH,LOW_TEXT_Y);
    for(int i=0;i<3;++i)cell(1,17+i,MIDI_X+float(i)*LOW_TEXT_PITCH,LOW_TEXT_Y);

    // 絵: 横 16 点 x 縦 16 点
    for(int y=0;y<16;++y)
    {
        const int row=y/8, line=y%8;
        for(int x=0;x<16;++x)
        {
            const bool on=x<15 ? bit(row,20+x/5,line,4-x%5) : bit(row,23,line,4);
            box(on,ICON_X+float(x)*ICON_COLUMN_PITCH,ICON_Y+float(y)*ICON_ROW_PITCH,DOT_SIZE,ICON_DOT_HEIGHT);
        }
    }

    // VOL と EXP: 桁 19 の 1 行目に入っている（左 2 点が VOL、右 2 点が EXP）
    for(int y=0;y<8;++y)
    {
        const float top=BAR_TOP+float(y)*BAR_PITCH;
        box(bit(0,19,y,4)||bit(0,19,y,3),VOL_CENTER-BAR_WIDTH/2,top,BAR_WIDTH,BAR_HEIGHT);
        box(bit(0,19,y,1)||bit(0,19,y,0),EXP_CENTER-BAR_WIDTH/2,top,BAR_WIDTH,BAR_HEIGHT);
    }

    const auto polar=[](float cx,float cy,float r,float degrees){
        const float a=juce::degreesToRadians(degrees);
        return Point{cx+r*std::sin(a),cy-r*std::cos(a)};
    };
    const auto ring=[&](float cx,float cy,float inner_radius,float outer_radius,float a0,float a1,juce::Colour colour){
        juce::Path shape;
        const int steps=juce::jlimit(8,180,int((a1-a0)/2));
        for(int i=0;i<=steps;++i){
            const auto p=polar(cx,cy,outer_radius,a0+(a1-a0)*float(i)/float(steps));
            if(i==0) shape.startNewSubPath(p); else shape.lineTo(p);
        }
        for(int i=steps;i>=0;--i)
            shape.lineTo(polar(cx,cy,inner_radius,a0+(a1-a0)*float(i)/float(steps)));
        shape.closeSubPath();
        g.setColour(colour);g.fillPath(shape);
    };
    // PAN: 下の開いた円弧（いつも点いている）と、45 度おきの針 7 本
    ring(PAN_X,PAN_Y,PAN_RADIUS-PAN_STROKE,PAN_RADIUS,-PAN_ARC,PAN_ARC,snapshot.lcdOn?ink:ghost);
    for(int k=0;k<7;++k)
    {
        const float degrees=-135.0f+45.0f*float(k), a=juce::degreesToRadians(degrees);
        const auto p0=polar(PAN_X,PAN_Y,PAN_NEEDLE_FROM,degrees), p1=polar(PAN_X,PAN_Y,PAN_NEEDLE_TO,degrees);
        const float nx=std::cos(a)*PAN_NEEDLE_HALF, ny=std::sin(a)*PAN_NEEDLE_HALF;
        juce::Path needle;
        needle.startNewSubPath(p0.x-nx,p0.y-ny);
        needle.lineTo(p1.x-nx,p1.y-ny);
        needle.lineTo(p1.x+nx,p1.y+ny);
        needle.lineTo(p0.x+nx,p0.y+ny);
        needle.closeSubPath();
        g.setColour(ctl(3,15-k)?ink:ghost);
        g.fillPath(needle);
    }
    // REV・CHO・VAR
    for(int n=0;n<3;++n)
    {
        const float cx=FAN_FIRST_X+float(n)*FAN_STEP;
        for(int k=0;k<8;++k)
        {
            const float r=float(k+1)*FAN_STEP_RADIUS;
            ring(cx,FAN_Y,r-FAN_STROKE,r+FAN_STROKE,-FAN_HALF_ANGLE,FAN_HALF_ANGLE,ctl(n,15-k)?ink:ghost);
        }
    }
    // KEY
    auto seven=[&](float x,unsigned seg){
        const float w=KEY_DIGIT_WIDTH,h=KEY_HEIGHT,t=KEY_STROKE,e=KEY_SEGMENT_GAP,mid=h/2;
        struct Corner{float u,v;};
        const auto part=[&](unsigned mask,std::initializer_list<Corner> corners){
            juce::Path shape;
            bool first=true;
            for(const auto& c:corners)
            {
                const Point at{x+c.u+KEY_SLANT*(1-c.v/h),KEY_Y+c.v};
                if(first)shape.startNewSubPath(at); else shape.lineTo(at);
                first=false;
            }
            shape.closeSubPath();
            g.setColour(seg&mask?ink:ghost);
            g.fillPath(shape);
        };
        part(1,{{e,0},{w-e,0},{w-e-t,t},{e+t,t}});                                 // a
        part(2,{{w,e},{w,mid-e},{w-t,mid-e-t/2},{w-t,e+t}});                       // b
        part(4,{{w,mid+e},{w,h-e},{w-t,h-e-t},{w-t,mid+e+t/2}});                   // c
        part(8,{{e,h},{w-e,h},{w-e-t,h-t},{e+t,h-t}});                             // d
        part(16,{{0,mid+e},{t,mid+e+t/2},{t,h-e-t},{0,h-e}});                      // e
        part(32,{{0,e},{t,e+t},{t,mid-e-t/2},{0,mid-e}});                          // f
        part(64,{{e,mid},{e+t/2,mid-t/2},{w-e-t/2,mid-t/2},{w-e,mid},{w-e-t/2,mid+t/2},{e+t/2,mid+t/2}}); // g
    };
    {
        const float cy=KEY_Y+KEY_HEIGHT/2, cx=KEY_SIGN_X+KEY_SIGN_WIDTH/2;
        box(ctl(1,0),KEY_SIGN_X,cy-KEY_STROKE/2,KEY_SIGN_WIDTH,KEY_STROKE);
        // + の縦棒は - の上下に分かれている
        const float arm=KEY_SIGN_HEIGHT/2-KEY_SIGN_GAP;
        box(ctl(0,0),cx-KEY_STROKE/2,cy-KEY_SIGN_HEIGHT/2,KEY_STROKE,arm);
        box(ctl(0,0),cx-KEY_STROKE/2,cy+KEY_SIGN_GAP,KEY_STROKE,arm);
        unsigned ten=ctl(0,1)?(1|8|16|64):0;
        if(ctl(1,1))ten|=2;
        if(ctl(0,7))ten|=4;
        if(ctl(1,4))ten|=32;
        unsigned one=0;
        const int cs[]={1,0,0,0,1,1,1},rs[]={6,6,3,2,2,7,3};
        for(int i=0;i<7;++i)if(ctl(cs[i],rs[i]))one|=1u<<i;
        seven(KEY_TEN_X,ten);
        seven(KEY_ONE_X,one);
    }
    auto triangle=[&](bool on,float ax,float ay,float bx,float by,float cx,float cy){
        juce::Path p;p.startNewSubPath(ax,ay);p.lineTo(bx,by);p.lineTo(cx,cy);p.closeSubPath();
        g.setColour(on?ink:ghost);g.fillPath(p);
    };
    // 一番上の矢印に当たる字はパネルにない
    const bool mode[]={false,ctl(1,5),ctl(0,4),ctl(0,5)};
    for(int k=0;k<4;++k)
        triangle(mode[k],MODE_X,MODE_Y[k]-MODE_HALF,MODE_X+MODE_WIDTH,MODE_Y[k],MODE_X,MODE_Y[k]+MODE_HALF);
    auto cursor=[&](bool on,float cx){
        triangle(on,cx-CURSOR_HALF,CURSOR_TIP-CURSOR_HEIGHT,cx+CURSOR_HALF,CURSOR_TIP-CURSOR_HEIGHT,cx,CURSOR_TIP);
    };
    cursor(ctl(2,2),CURSOR_MIDI);
    cursor(ctl(2,3),VOL_CENTER);
    cursor(ctl(2,4),EXP_CENTER);
    cursor(ctl(2,5),PAN_X);
    cursor(ctl(2,6),FAN_FIRST_X);
    cursor(ctl(2,7),FAN_FIRST_X+FAN_STEP);
    cursor(ctl(3,6),FAN_FIRST_X+2*FAN_STEP);
    cursor(ctl(3,7),CURSOR_KEY);
    cursor(ctl(2,1),CURSOR_BANK);
    cursor(ctl(2,0),CURSOR_PROGRAM);
}
