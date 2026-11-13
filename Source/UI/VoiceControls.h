#pragma once
#include "Style.h"

namespace phoqer::ui {
juce::Colour themeColour(int voice,float position) noexcept;
juce::Colour softColour(juce::Colour colour,float amount) noexcept;

class VoiceSlot98 final : public juce::Button {
public:
    explicit VoiceSlot98(int voice):juce::Button("VOICE"),index(voice){setWantsKeyboardFocus(false);}
    int index;
    bool selected=false,available=true;
    juce::String label;
    static juce::Path letteringPath(const juce::String& caption,juce::Rectangle<float> area,float scale,juce::Point<float> originPixels={})
    {
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText(pixelFont(11,true),caption,0,0);
        juce::Path letters;glyphs.createPath(letters);
        const auto bounds=letters.getBounds();
        const auto snapX=[=](float p){return (std::round(p*scale+originPixels.x)-originPixels.x)/scale;};
        const auto snapY=[=](float p){return (std::round(p*scale+originPixels.y)-originPixels.y)/scale;};
        letters.applyTransform(juce::AffineTransform::translation(
            snapX(area.getCentreX()-bounds.getWidth()*.5f)-bounds.getX(),
            snapY(area.getCentreY()-bounds.getHeight()*.5f)-bounds.getY()));
        // Keep the pixel font's outlines on physical pixels, including125/150%.
        juce::Path aligned;juce::Path::Iterator point(letters);
        while(point.next())
            switch(point.elementType)
            {
                case juce::Path::Iterator::startNewSubPath:aligned.startNewSubPath(snapX(point.x1),snapY(point.y1));break;
                case juce::Path::Iterator::lineTo:aligned.lineTo(snapX(point.x1),snapY(point.y1));break;
                case juce::Path::Iterator::quadraticTo:aligned.quadraticTo(snapX(point.x1),snapY(point.y1),snapX(point.x2),snapY(point.y2));break;
                case juce::Path::Iterator::cubicTo:aligned.cubicTo(snapX(point.x1),snapY(point.y1),snapX(point.x2),snapY(point.y2),snapX(point.x3),snapY(point.y3));break;
                case juce::Path::Iterator::closePath:aligned.closeSubPath();break;
            }
        return aligned;
    }
    void paintButton(juce::Graphics& g,bool,bool) override {
        const auto area=getLocalBounds().toFloat();
        g.fillAll(juce::Colour(14,8,24));
        const auto scale=juce::jmax(.25f,g.getInternalContext().getPhysicalPixelScaleFactor());
        const auto* root=getTopLevelComponent();
        const auto origin=root->getLocalPoint(this,juce::Point<float>());
        const auto unit=root->getLocalPoint(this,juce::Point<float>(1,0));
        const auto layoutScale=juce::jmax(.01f,unit.getDistanceFrom(origin));
        const auto letters=letteringPath(available?label:juce::String("+ ADD"),area,scale,origin*(scale/layoutScale));
        if(!available){g.setColour(juce::Colour(90,86,100));g.fillPath(letters);return;}
        if(selected){juce::ColourGradient glow(themeColour(index,0).withMultipliedBrightness(.28f),4,0,themeColour(index,1).withMultipliedBrightness(.28f),area.getRight()-4,0,false);if(index==3)glow.addColour(.5,themeColour(3,.5).withMultipliedBrightness(.28f));g.setGradientFill(glow);g.fillRect(area.reduced(4,3));}
        const auto bounds=letters.getBounds();
        const auto colour=[this](float t){auto c=themeColour(index,t);return selected?c:softColour(c.withMultipliedBrightness(.72f),.60f);};
        juce::ColourGradient ink(colour(0),bounds.getX(),0,colour(1),bounds.getRight(),0,false);
        for(int stop=1;stop<32;++stop)ink.addColour(stop/32.0,colour(stop/32.0f));
        g.setGradientFill(ink);g.fillPath(letters);
    }
};
class ActionButton98 final : public juce::TextButton {
public:
    explicit ActionButton98(const juce::String& caption):juce::TextButton(caption){setWantsKeyboardFocus(false);}
    bool primary=false;
    void paintButton(juce::Graphics& g,bool,bool down) override {
        auto area=getLocalBounds().toFloat();
        if(primary){g.setColour(juce::Colours::black);g.drawRect(area,1);area=area.reduced(1);}
        button98(g,area,down);
        if(getButtonText()=="=")
        {
            const float x=std::floor(getWidth()*.5f-6),y=std::floor(getHeight()*.5f-4);
            g.setColour(juce::Colours::black);for(int i=0;i<3;++i)g.fillRect(x,y+i*4,12.0f,1.0f);return;
        }
        drawText(g,getButtonText(),getLocalBounds().toFloat(),pixelFont(9,true),isEnabled()?juce::Colours::black:win98::shadow,juce::Justification::centred);
    }
};
}
