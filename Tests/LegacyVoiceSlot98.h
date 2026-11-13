#pragma once
#include "UI/VoiceControls.h"
namespace phoqer::ui {
class LegacyVoiceSlot98 final : public juce::Button {
public:
    explicit LegacyVoiceSlot98(int voice):juce::Button("VOICE"),index(voice){setWantsKeyboardFocus(false);}
    int index;
    bool selected=false,available=true;
    juce::String label;
    void paintButton(juce::Graphics& g,bool,bool) override {
        const auto area=getLocalBounds().toFloat();
        g.fillAll(juce::Colour(14,8,24));
        if(!available){drawText(g,"+ ADD",area,pixelFont(8,true),juce::Colour(90,86,100),juce::Justification::centred);return;}
        if(selected){juce::ColourGradient glow(themeColour(index,0).withMultipliedBrightness(.28f),4,0,themeColour(index,1).withMultipliedBrightness(.28f),area.getRight()-4,0,false);if(index==3)glow.addColour(.5,themeColour(3,.5).withMultipliedBrightness(.28f));g.setGradientFill(glow);g.fillRect(area.reduced(4,3));}
        juce::Image lettering(juce::Image::ARGB,getWidth(),getHeight(),true);
        {juce::Graphics ink(lettering);drawText(ink,label,area,pixelFont(8,true),juce::Colours::white,juce::Justification::centred);}
        // Pixel masks retain the exact font grid at each integer editor scale.
        int x0=getWidth(),x1=0;
        for(int y=0;y<getHeight();++y)for(int x=0;x<getWidth();++x)if(lettering.getPixelAt(x,y).getAlpha()>110){x0=juce::jmin(x0,x);x1=juce::jmax(x1,x);}
        for(int y=0;y<getHeight();++y)for(int x=0;x<getWidth();++x){const auto a=lettering.getPixelAt(x,y).getAlpha();if(a>110){auto c=themeColour(index,static_cast<float>(x-x0)/juce::jmax(1,x1-x0));if(!selected)c=softColour(c.withMultipliedBrightness(.72f),.60f);lettering.setPixelAt(x,y,c);}else lettering.setPixelAt(x,y,juce::Colours::transparentBlack);}
        g.drawImageAt(lettering,0,0);
    }
};
}
