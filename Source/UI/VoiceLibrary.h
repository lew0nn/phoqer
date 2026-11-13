#pragma once
#include "Logo.h"
#include "Style.h"
#include <array>
#include <vector>

namespace phoqer::ui {

inline const std::array<juce::Image,4>& voiceLibraryBadges()
{
    static const auto images=[]
    {
        std::array<juce::Image,4> badges;
        badges[0]=loadAssetImage("phoqer-app-icon-32.png");
        for(int voice=1;voice<3;++voice)
        {
            auto result=badges[0].createCopy();
            const auto source=paletteFor(0);auto target=paletteFor(voice);
            const auto tone=[](const Palette& p,int family,float t)
            {
                switch(family){case 0:return p.sky.interpolatedWith(p.accent,t);case 1:return p.accent.interpolatedWith(p.secondary,t);case 2:return p.secondary.interpolatedWith(juce::Colours::white,t);case 3:return juce::Colour(10,10,10).interpolatedWith(juce::Colours::white,t);default:return p.accent.interpolatedWith(juce::Colours::white,t);}
            };
            std::vector<std::pair<juce::Colour,juce::Colour>> colours;
            for(int family=0;family<5;++family)for(int step=0;step<=64;++step)
            {
                const float t=step/64.f;auto to=tone(target,family,t);
                colours.emplace_back(tone(source,family,t),to);
            }
            for(int y=0;y<result.getHeight();++y)for(int x=0;x<result.getWidth();++x)
            {
                const auto c=badges[0].getPixelAt(x,y);if(c.getAlpha()==0)continue;
                int best=1000000;juce::Colour selected;
                for(const auto& pair:colours)
                {
                    const int dr=c.getRed()-pair.first.getRed(),dg=c.getGreen()-pair.first.getGreen(),db=c.getBlue()-pair.first.getBlue();
                    const int distance=dr*dr+dg*dg+db*db;if(distance<best){best=distance;selected=pair.second;}
                }
                result.setPixelAt(x,y,selected.withAlpha(c.getAlpha()));
            }
            badges[static_cast<size_t>(voice)]=result;
        }
        badges[3]=loadAssetImage("kyrax-badge-32.png");     // hand-tuned: gold > hot pink > violet, whiter whiskers
        return badges;
    }();
    return images;
}

inline juce::Rectangle<float> paintVoiceLibraryFrame(juce::Graphics& g,juce::Rectangle<float> bounds)
{
    g.setColour(win98::face);g.fillRect(bounds);bevel(g,bounds,true);
    const auto bar=windowTitleBar(bounds);
    juce::ColourGradient gradient(juce::Colour(80,30,220),bar.getX(),0,juce::Colour(210,132,198),bar.getRight(),0,false);
    gradient.addColour(.6,juce::Colour(169,76,187));g.setGradientFill(gradient);g.fillRect(bar);
    drawText(g,"VOICES.EXE",{8,3,204,16},pixelFont(11,true),juce::Colours::white);
    return windowClient(bounds);
}

inline void paintVoiceLibraryContent(juce::Graphics& g)
{
    drawText(g,"YOUR VOICES",{10,27,210,15},pixelFont(10,true),juce::Colours::black);
    sunken(g,{10,45,218,172},juce::Colour(173,169,178));
}

class VoiceLibraryRow98 final : public juce::Button {
public:
    explicit VoiceLibraryRow98(int voice):juce::Button("VOICE"),index(voice)
    {setWantsKeyboardFocus(false);setMouseClickGrabsKeyboardFocus(false);}
    void setVoiceState(bool isSelected,bool isActive,bool isAvailable)
    {if(selected!=isSelected||active!=isActive||available!=isAvailable){selected=isSelected;active=isActive;available=isAvailable;repaint();}}
    void setDetail(const juce::String& value){if(detail!=value){detail=value;repaint();}}   // replaces the description, e.g. the key state
    bool isSelectedVoice() const noexcept{return selected;}
    bool isActiveVoice() const noexcept{return active;}
    void paintButton(juce::Graphics& g,bool,bool) override
    {
        if(selected){g.fillAll(juce::Colour(104,83,117));g.setColour(juce::Colour(56,37,72));g.fillRect(0,0,3,getHeight());}
        g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);g.setOpacity(available?1.f:.4f);
        g.drawImageAt(voiceLibraryBadges()[static_cast<size_t>(index)],10,4);g.setOpacity(1);
        const char* names[]={"BURP","SQUEAL","GROAN","KYRAX"};
        const char* descriptions[]={"LOW + ROUGH","BRIGHT YELPS","WARM MOAN","WIDE VOWELS"};
        const auto ink=selected?juce::Colours::white:juce::Colour(43,32,51);
        const auto detailInk=selected?juce::Colour(229,216,238):juce::Colour(81,73,88);
        drawText(g,names[index],{52,4,152,16},pixelFont(11,true),ink);
        drawText(g,detail.isNotEmpty()?detail:juce::String(descriptions[index]),{52,21,152,12},pixelFont(8),detailInk);
        if(active){g.setColour(juce::Colour(53,32,78));g.fillRect(158,4,48,12);drawText(g,"ACTIVE",{158,4,48,12},pixelFont(8,true),juce::Colour(233,213,251),juce::Justification::centred);}
    }
private:
    int index;bool selected=false,active=false,available=true;juce::String detail;
};
}
