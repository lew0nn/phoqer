#pragma once
#include "UI/Logo.h"

namespace pocketIconStudies {
using namespace identityStudies;

class Assets {
public:
    explicit Assets(const juce::File& castFile)
    {
        const auto cast=juce::ImageFileFormat::loadFrom(castFile);
        if(!cast.isValid()||cast.getWidth()!=760||cast.getHeight()!=280)throw std::runtime_error("Expected the existing README cast image");
        const juce::Rectangle<int> regions[]={{280,164,176,116},{520,176,176,104},{60,176,160,104}};
        for(int voice=0;voice<3;++voice)
        {
            const auto r=regions[voice];auto image=juce::Image(juce::Image::ARGB,r.getWidth()/4,r.getHeight()/4,true);
            // The README artwork is native pixel art enlarged exactly4x.
            // Sample one pixel per4x4 block to reuse its existing geometry.
            for(int y=0;y<image.getHeight();++y)for(int x=0;x<image.getWidth();++x)image.setPixelAt(x,y,cast.getPixelAt(r.getX()+x*4+2,r.getY()+y*4+2));
            heads[static_cast<size_t>(voice)]=image;
        }
        heads[3]=heads[2].createCopy();
        const juce::Colour ramp[]={rgb(0x100432),rgb(0x5a28dc),rgb(0xd06bc3),rgb(0xbea15e).interpolatedWith(rgb(0xfaec3c),.16f),rgb(0xe7cda0).interpolatedWith(rgb(0xffffd2),.16f)};
        constexpr float stops[]={0,.3f,.55f,.8f,1};
        for(int y=0;y<heads[3].getHeight();++y)for(int x=0;x<heads[3].getWidth();++x)
        {
            const auto c=heads[3].getPixelAt(x,y);if(c.getAlpha()==0||c.getBrightness()<.055f)continue;
            const float value=(.299f*c.getRed()+.587f*c.getGreen()+.114f*c.getBlue())/255;
            int stop=0;while(stop<3&&value>stops[stop+1])++stop;
            heads[3].setPixelAt(x,y,ramp[stop].interpolatedWith(ramp[stop+1],(value-stops[stop])/(stops[stop+1]-stops[stop])).withAlpha(c.getAlpha()));
        }
        badges[0]=loadAssetImage("phoqer-app-icon-32.png");
        if(!badges[0].isValid())throw std::runtime_error("Existing app icon missing");
        for(int voice=1;voice<4;++voice)badges[static_cast<size_t>(voice)]=badgeFor(voice);
    }
    std::array<juce::Image,4> heads,badges;
private:
    juce::Image badgeFor(int voice)
    {
        auto result=badges[0].createCopy();
        const auto source=paletteFor(0);auto target=paletteFor(voice);
        if(voice==3)target.secondary=rgb(0xbea15e).interpolatedWith(rgb(0xfaec3c),.16f);
        const auto tone=[](const Palette& p,int family,float t){switch(family){case 0:return p.sky.interpolatedWith(p.accent,t);case 1:return p.accent.interpolatedWith(p.secondary,t);case 2:return p.secondary.interpolatedWith(juce::Colours::white,t);case 3:return rgb(0x0a0a0a).interpolatedWith(juce::Colours::white,t);default:return p.accent.interpolatedWith(juce::Colours::white,t);}};
        std::vector<std::pair<juce::Colour,juce::Colour>> colours;
        for(int family=0;family<5;++family)for(int step=0;step<=64;++step)
        {
            const float t=step/64.f;auto to=tone(target,family,t);
            if(voice==3&&family==1)to=t<.5f?target.accent.interpolatedWith(rgb(0xd06bc3),t*2):rgb(0xd06bc3).interpolatedWith(target.secondary,t*2-1);
            colours.emplace_back(tone(source,family,t),to);
        }
        // Reuse the exact startup icon shape/alpha; only its voice palette varies.
        for(int y=0;y<result.getHeight();++y)for(int x=0;x<result.getWidth();++x)
        {
            const auto c=badges[0].getPixelAt(x,y);if(c.getAlpha()==0)continue;
            int best=1000000;juce::Colour selected;
            for(const auto& pair:colours){const int dr=c.getRed()-pair.first.getRed(),dg=c.getGreen()-pair.first.getGreen(),db=c.getBlue()-pair.first.getBlue();const int distance=dr*dr+dg*dg+db*db;if(distance<best){best=distance;selected=pair.second;}}
            result.setPixelAt(x,y,selected.withAlpha(c.getAlpha()));
        }
        return result;
    }
};

class Window final : public juce::Component {
public:
    Window(int style,const Assets& art):kind(style),assets(art){setSize(238,298);}
    void paint(juce::Graphics& g) override
    {
        g.fillAll(win98::face);bevel(g,getLocalBounds().toFloat(),true);const auto bar=windowTitleBar(getLocalBounds().toFloat());
        juce::ColourGradient gradient(rgb(0x501edc),bar.getX(),0,rgb(0xd284c6),bar.getRight(),0,false);gradient.addColour(.6,rgb(0xa94cbb));g.setGradientFill(gradient);g.fillRect(bar);
        text(g,"VOICES.EXE",{8,3,204,16},11,true,juce::Colours::white);button98(g,{218,5,14,12},false);text(g,"X",{218,4,14,14},11,true,juce::Colours::black,juce::Justification::centred);
        // Surface studies: original, charcoal, violet and classic gray.
        const int surfaces[][7]={
            {0xecebef,0xd8c6e8,0x402454,0x544c5b,0x402454,0x544c5b,0x77469f},
            {0x202024,0x40364e,0xe6e0ec,0xaaa3b4,0xf3e7ff,0xd0bddf,0xb48ddb},
            {0x171020,0x352346,0xe7d8f4,0xae9bbb,0xf3e5ff,0xd4bce8,0xb286d8},
            {0xada9b2,0x685375,0x2b2033,0x514958,0xffffff,0xe5d8ee,0x382548}
        };
        const auto* surface=surfaces[kind>=3?kind-2:0];
        text(g,"YOUR VOICES",{10,27,210,15},10,true);sunken(g,{10,45,218,172},rgb(surface[0]));
        for(int voice=0;voice<4;++voice)
        {
            const int y=48+voice*41;
            if(voice==3){g.setColour(rgb(surface[1]));g.fillRect(13,y,212,40);g.setColour(rgb(surface[6]));g.fillRect(13,y,3,40);}
            if(kind==2)
            {
                // A's native-size mascot, with B's contained dark backing.
                g.setColour(paletteFor(voice).accent.withMultipliedBrightness(.42f));
                g.fillRect(17,y+3,44,34);g.fillRect(15,y+5,48,30);
                g.setColour(rgb(0x15101e));g.fillRect(18,y+4,42,32);g.fillRect(16,y+6,46,28);
            }
            const auto& image=(kind==1||kind>=3?assets.badges:assets.heads)[static_cast<size_t>(voice)];
            g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);g.drawImageAt(image,17+(44-image.getWidth())/2,y+(40-image.getHeight())/2);
            text(g,names[voice],{65,y+4,152,16},11,true,rgb(surface[voice==3?4:2]));text(g,shortDescriptions[voice],{65,y+21,152,12},8,false,rgb(surface[voice==3?5:3]));
            if(voice==3)tag(g,{171,y+4,48,12},"ACTIVE",true);
        }
        checked(g,14,225,"IN QUICK BAR");push(g,{10,247,104,22},"PREVIEW");push(g,{122,247,106,22},"USE",true);push(g,{10,276,218,16},"IMPORT VOICE...");
    }
private:int kind;const Assets& assets;
};

class Stage final : public juce::Component {
    PhoqerView& view;
public:
    Stage(PhoqerView& parent,int kind,const Assets& art):view(parent),panel(kind,art){setSize(760,730);addAndMakeVisible(panel);panel.setTopLeftPosition(506,96);}
    void paint(juce::Graphics& g) override{view.paintEntireComponent(g,true);}
    Window panel;
};
class Lineup final : public juce::Component {
    PortraitRenderer portrait;miniVoiceStudies::Window previous{1,portrait};Window heads,badges,hybrid;
public:
    explicit Lineup(const Assets& art):heads(0,art),badges(1,art),hybrid(2,art){setSize(1096,354);int x=16;for(auto* panel:std::array<juce::Component*,4>{&previous,&heads,&badges,&hybrid}){addAndMakeVisible(*panel);panel->setTopLeftPosition(x,42);x+=270;}}
    void paint(juce::Graphics& g) override{g.fillAll(rgb(0xd4d4d6));const char* titles[]={"PREVIOUS PORTRAITS","A - README MASCOTS","B - APP BADGES","C - HYBRID"};for(int i=0;i<4;++i)text(g,titles[i],{16+i*270,12,250,22},13,true,rgb(0x302039));}
};
inline void render(const juce::File& voice,const juce::File& folder)
{
    const juce::File cast(juce::SystemStats::getEnvironmentVariable("PHOQER_ICON_CAST",{}));Assets assets(cast);
    auto processor=std::make_unique<PhoqerAudioProcessor>();juce::String error;if(!processor->getVoices().loadVoiceFile(voice,error))throw std::runtime_error(error.toStdString());
    std::unique_ptr<juce::AudioProcessorEditor> base(processor->createEditor());auto* editor=dynamic_cast<PhoqerEditor*>(base.get());if(!editor)throw std::runtime_error("Missing editor");
    WindowControls controls;controls.audioSettings=controls.minimise=controls.toggleFullscreen=controls.close=[]{};editor->setWindowControls(controls);processor->getVoices().useLoadedVoice();editor->tick(1000);
    for(int kind=0;kind<3;++kind)
    {
        Stage stage(editor->getView(),kind,assets);const auto letter=juce::String::charToString(static_cast<juce::juce_wchar>('a'+kind));
        for(int scale:{1,2}){const auto suffix=letter+"-"+juce::String(scale)+"x.png";save(stage.createComponentSnapshot(stage.getLocalBounds(),true,static_cast<float>(scale)),folder.getChildFile("window-"+suffix));save(stage.panel.createComponentSnapshot(stage.panel.getLocalBounds(),true,static_cast<float>(scale)),folder.getChildFile("panel-"+suffix));}
    }
    Lineup lineup(assets);for(int scale:{1,2})save(lineup.createComponentSnapshot(lineup.getLocalBounds(),true,static_cast<float>(scale)),folder.getChildFile("lineup-"+juce::String(scale)+"x.png"));
    for(int voiceIndex=0;voiceIndex<4;++voiceIndex){save(assets.heads[static_cast<size_t>(voiceIndex)],folder.getChildFile("icon-readme-"+juce::String(voiceIndex)+".png"));save(assets.badges[static_cast<size_t>(voiceIndex)],folder.getChildFile("icon-app-"+juce::String(voiceIndex)+".png"));}
}
class BackgroundLineup final : public juce::Component {
    Window previous,charcoal,violet,gray;
public:
    explicit BackgroundLineup(const Assets& art):previous(1,art),charcoal(3,art),violet(4,art),gray(5,art){setSize(1096,354);int x=16;for(auto* panel:std::array<juce::Component*,4>{&previous,&charcoal,&violet,&gray}){addAndMakeVisible(*panel);panel->setTopLeftPosition(x,42);x+=270;}}
    void paint(juce::Graphics& g) override{g.fillAll(rgb(0xd4d4d6));const char* labels[]={"PREVIOUS WHITE","1 - CHARCOAL","2 - DEEP VIOLET","3 - CLASSIC GRAY"};for(int i=0;i<4;++i)text(g,labels[i],{16+i*270,12,250,22},13,true,rgb(0x302039));}
};
inline void renderBackgrounds(const juce::File& voice,const juce::File& folder)
{
    const juce::File cast(juce::SystemStats::getEnvironmentVariable("PHOQER_ICON_CAST",{}));Assets assets(cast);
    auto processor=std::make_unique<PhoqerAudioProcessor>();juce::String error;if(!processor->getVoices().loadVoiceFile(voice,error))throw std::runtime_error(error.toStdString());
    std::unique_ptr<juce::AudioProcessorEditor> base(processor->createEditor());auto* editor=dynamic_cast<PhoqerEditor*>(base.get());if(!editor)throw std::runtime_error("Missing editor");
    WindowControls controls;controls.audioSettings=controls.minimise=controls.toggleFullscreen=controls.close=[]{};editor->setWindowControls(controls);processor->getVoices().useLoadedVoice();editor->tick(1000);
    for(int kind=3;kind<6;++kind)
    {
        Stage stage(editor->getView(),kind,assets);
        for(int scale:{1,2}){const auto suffix=juce::String(kind-2)+"-"+juce::String(scale)+"x.png";save(stage.createComponentSnapshot(stage.getLocalBounds(),true,static_cast<float>(scale)),folder.getChildFile("window-"+suffix));save(stage.panel.createComponentSnapshot(stage.panel.getLocalBounds(),true,static_cast<float>(scale)),folder.getChildFile("panel-"+suffix));}
    }
    BackgroundLineup lineup(assets);for(int scale:{1,2})save(lineup.createComponentSnapshot(lineup.getLocalBounds(),true,static_cast<float>(scale)),folder.getChildFile("lineup-"+juce::String(scale)+"x.png"));
    for(int v=0;v<4;++v)save(assets.badges[static_cast<size_t>(v)],folder.getChildFile("icon-app-"+juce::String(v)+".png"));
}
}
