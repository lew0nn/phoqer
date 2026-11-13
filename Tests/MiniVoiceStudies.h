#pragma once

namespace miniVoiceStudies {
using namespace identityStudies;
inline const char* labels[]={"A - MINI CARDS","B - POCKET LIST","C - FOUR TILES","D - ICON FOLDER","E - TINY INSPECTOR"};
inline const juce::Point<int> sizes[]={{352,216},{238,298},{264,250},{322,198},{302,202}};

class Window final : public juce::Component {
public:
    Window(int type,const PortraitRenderer& renderer):kind(type),portrait(renderer){setSize(sizes[kind].x,sizes[kind].y);}
    void paint(juce::Graphics& g) override
    {
        g.fillAll(win98::face);bevel(g,getLocalBounds().toFloat(),true);
        auto bar=windowTitleBar(getLocalBounds().toFloat());
        juce::ColourGradient gradient(rgb(0x501edc),bar.getX(),0,rgb(0xd284c6),bar.getRight(),0,false);gradient.addColour(.6,rgb(0xa94cbb));g.setGradientFill(gradient);g.fillRect(bar);
        text(g,"VOICES.EXE",{8,3,getWidth()-34,16},11,true,juce::Colours::white);
        button98(g,{static_cast<float>(getWidth()-20),5,14,12},false);text(g,"X",{getWidth()-20,4,14,14},11,true,juce::Colours::black,juce::Justification::centred);
        switch(kind){case 0:cards(g);break;case 1:pocket(g);break;case 2:tiles(g);break;case 3:folder(g);break;default:inspector(g);break;}
    }
private:
    void art(juce::Graphics& g,int voice,juce::Rectangle<int> area)
    {
        phoqer::ui::Expression expression{};expression.face.eyeOpen=.6f;
        g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);g.drawImageAt(portrait.render(voice,expression,area.getWidth(),area.getHeight()),area.getX(),area.getY());
    }
    void active(juce::Graphics& g,juce::Rectangle<int> area){tag(g,area,"ACTIVE",true);}
    void cards(juce::Graphics& g)
    {
        text(g,"YOUR VOICES",{10,27,200,14},10,true);text(g,"4",{308,27,32,14},9,false,rgb(0x54505a),juce::Justification::centredRight);
        for(int i=0;i<4;++i)
        {
            const int x=10+i*84;g.setColour(i==3?rgb(0x30203f):rgb(0x19141f));g.fillRect(x,47,80,103);
            if(i==3){g.setColour(rgb(0xab7cce));g.drawRect(x,47,80,103,2);}
            art(g,i,{x+8,54,64,56});text(g,names[i],{x+2,115,76,15},11,true,rgb(0xe3d1f2),juce::Justification::centred);
            if(i==3)active(g,{x+4,133,72,12});else text(g,"INCLUDED",{x+2,132,76,14},8,false,rgb(0xaa9bb9),juce::Justification::centred);
        }
        text(g,"KYRAX / WIDE VOWELS",{10,158,204,16},9,true,rgb(0x5d288e));checked(g,232,158,"QUICK BAR");
        push(g,{10,183,96,24},"IMPORT...");push(g,{114,183,110,24},"PREVIEW");push(g,{232,183,110,24},"USE",true);
    }
    void pocket(juce::Graphics& g)
    {
        text(g,"YOUR VOICES",{10,27,210,15},10,true);
        sunken(g,{10,45,218,172},rgb(0xecebef));
        for(int i=0;i<4;++i)
        {
            const int y=48+i*41;
            if(i==3){g.setColour(rgb(0xd8c6e8));g.fillRect(13,y,212,40);g.setColour(rgb(0x77469f));g.fillRect(13,y,3,40);}
            art(g,i,{21,y+4,34,30});text(g,names[i],{65,y+4,152,16},11,true,rgb(0x402454));
            text(g,shortDescriptions[i],{65,y+21,152,12},8,false,rgb(0x544c5b));if(i==3)active(g,{171,y+4,48,12});
        }
        checked(g,14,225,"IN QUICK BAR");push(g,{10,247,104,22},"PREVIEW");push(g,{122,247,106,22},"USE",true);push(g,{10,276,218,16},"IMPORT VOICE...");
    }
    void tiles(juce::Graphics& g)
    {
        text(g,"YOUR VOICES",{10,27,244,15},10,true);
        for(int i=0;i<4;++i)
        {
            const int x=10+(i%2)*126,y=47+(i/2)*77;
            g.setColour(i==3?rgb(0x30203f):rgb(0x19141f));g.fillRect(x,y,118,70);
            if(i==3){g.setColour(rgb(0xab7cce));g.drawRect(x,y,118,70,2);}
            art(g,i,{x+6,y+6,44,42});text(g,names[i],{x+55,y+10,60,16},10,true,rgb(0xe3d1f2));
            text(g,i==3?"LOADED":"INCLUDED",{x+55,y+29,60,13},7,false,rgb(0xb4a1c5));
            if(i==3)active(g,{x+6,y+53,106,12});
        }
        checked(g,10,202,"IN QUICK BAR");push(g,{10,225,76,17},"IMPORT...");push(g,{92,225,76,17},"PREVIEW");push(g,{174,225,80,17},"USE",true);
    }
    void folder(juce::Graphics& g)
    {
        text(g,"INSTALLED",{10,27,180,14},9,true);text(g,"4 ITEMS",{202,27,110,14},9,false,rgb(0x54505a),juce::Justification::centredRight);
        sunken(g,{10,47,302,86},rgb(0xecebef));
        for(int i=0;i<4;++i)
        {
            const int x=16+i*74;
            // File-like portrait icons, distinct from the gallery cards.
            g.setColour(juce::Colours::white);g.fillRect(x+15,55,44,42);g.setColour(rgb(0x6d6376));g.drawRect(x+15,55,44,42,1);
            art(g,i,{x+18,59,36,32});g.setColour(juce::Colours::white);g.fillRect(x+50,55,8,7);g.setColour(rgb(0x6d6376));g.drawLine(static_cast<float>(x+50),55.f,static_cast<float>(x+58),62.f,1);
            if(i==3){g.setColour(rgb(0x503366));g.fillRect(x+2,104,68,17);}
            text(g,names[i],{x+2,104,68,17},10,true,i==3?juce::Colours::white:rgb(0x302039),juce::Justification::centred);
        }
        text(g,"KYRAX / ACTIVE",{10,140,160,15},9,true,rgb(0x5d288e));checked(g,202,140,"QUICK BAR");
        push(g,{10,165,94,24},"IMPORT...");push(g,{112,165,96,24},"PREVIEW");push(g,{216,165,96,24},"USE",true);
    }
    void inspector(juce::Graphics& g)
    {
        sunken(g,{10,36,104,112},rgb(0x17131e));
        for(int i=0;i<4;++i){const int y=39+i*26;if(i==3){g.setColour(rgb(0x49305f));g.fillRect(13,y,98,25);}text(g,names[i],{22,y,83,25},11,true,i==3?rgb(0xf2dbff):rgb(0xb6a4c7));}
        text(g,"KYRAX",{130,33,100,19},13,true,rgb(0x5d288e));active(g,{238,36,54,12});
        art(g,3,{163,58,72,64});text(g,"WIDE VOWELS",{128,130,164,14},9,false,rgb(0x4c4355),juce::Justification::centred);
        checked(g,10,155,"IN QUICK BAR");
        push(g,{10,177,86,17},"IMPORT...");push(g,{104,177,90,17},"PREVIEW");push(g,{202,177,90,17},"USE",true);
    }
    int kind;const PortraitRenderer& portrait;
};

class Stage final : public juce::Component {
private:
    PhoqerView& view;PortraitRenderer portrait;
public:
    Stage(PhoqerView& parent,int kind):view(parent),panel(kind,portrait)
    {setSize(760,730);addAndMakeVisible(panel);panel.setTopLeftPosition(744-panel.getWidth(),96);}
    void paint(juce::Graphics& g) override {view.paintEntireComponent(g,true);}
    Window panel;
};

class Lineup final : public juce::Component {
public:
    Lineup()
    {
        setSize(1120,688);
        for(int i=0;i<5;++i){windows[static_cast<size_t>(i)]=std::make_unique<Window>(i,portrait);addAndMakeVisible(*windows[static_cast<size_t>(i)]);windows[static_cast<size_t>(i)]->setTopLeftPosition(16+(i%3)*368,42+(i/3)*338);}
    }
    void paint(juce::Graphics& g) override {g.fillAll(rgb(0xd4d4d6));for(int i=0;i<5;++i)text(g,labels[i],{16+(i%3)*368,12+(i/3)*338,350,22},14,true,rgb(0x302039));}
private:
    PortraitRenderer portrait;std::array<std::unique_ptr<Window>,5> windows;
};

inline void render(const juce::File& voice,const juce::File& folder)
{
    auto processor=std::make_unique<PhoqerAudioProcessor>();juce::String error;
    if(!processor->getVoices().loadVoiceFile(voice,error))throw std::runtime_error(error.toStdString());
    std::unique_ptr<juce::AudioProcessorEditor> base(processor->createEditor());auto* editor=dynamic_cast<PhoqerEditor*>(base.get());if(!editor)throw std::runtime_error("Missing editor");
    WindowControls controls;controls.audioSettings=controls.minimise=controls.toggleFullscreen=controls.close=[]{};editor->setWindowControls(controls);processor->getVoices().useLoadedVoice();editor->tick(1000);
    for(int kind=0;kind<5;++kind)
    {
        Stage stage(editor->getView(),kind);const auto letter=juce::String::charToString(static_cast<juce::juce_wchar>('a'+kind));
        for(int scale:{1,2})
        {
            const auto suffix=letter+"-"+juce::String(scale)+"x.png";
            save(stage.createComponentSnapshot(stage.getLocalBounds(),true,static_cast<float>(scale)),folder.getChildFile("window-"+suffix));
            save(stage.panel.createComponentSnapshot(stage.panel.getLocalBounds(),true,static_cast<float>(scale)),folder.getChildFile("panel-"+suffix));
        }
    }
    Lineup lineup;for(int scale:{1,2})save(lineup.createComponentSnapshot(lineup.getLocalBounds(),true,static_cast<float>(scale)),folder.getChildFile("lineup-"+juce::String(scale)+"x.png"));
}
}
