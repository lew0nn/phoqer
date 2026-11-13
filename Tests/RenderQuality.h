#pragma once
#include "LegacyVoiceSlot98.h"
#include "LegacyWindow98.h"

namespace renderQuality {
inline void save(const juce::Image& image,const juce::File& file)
{
    if(file.exists())throw std::runtime_error("Use a fresh quality output folder");
    juce::FileOutputStream output(file);
    if(!output.openedOk()||!juce::PNGImageFormat().writeImageToStream(image,output))throw std::runtime_error("Cannot write quality PNG");
}
template <class Slot> inline juce::Image selector(float scale)
{
    juce::Component strip;strip.setSize(248,22);
    std::array<std::unique_ptr<Slot>,4> slots;
    const char* names[]={"BURP","SQUEAL","GROAN","KYRAX"};
    for(size_t i=0;i<4;++i){slots[i]=std::make_unique<Slot>(static_cast<int>(i));slots[i]->label=names[i];slots[i]->selected=i==3;slots[i]->setBounds(static_cast<int>(i)*62,0,62,22);strip.addAndMakeVisible(*slots[i]);}
    return strip.createComponentSnapshot(strip.getLocalBounds(),true,scale);
}
inline void render(const juce::File& voice,const juce::File& folder)
{
    auto processor=std::make_unique<PhoqerAudioProcessor>();juce::String error;
    if(!processor->getVoices().loadVoiceFile(voice,error))throw std::runtime_error(error.toStdString());
    std::unique_ptr<juce::AudioProcessorEditor> base(processor->createEditor());
    auto* editor=dynamic_cast<phoqer::ui::PhoqerEditor*>(base.get());
    if(editor==nullptr)throw std::runtime_error("Editor missing");
    phoqer::ui::WindowControls controls;controls.audioSettings=controls.minimise=controls.toggleFullscreen=controls.close=[]{};editor->setWindowControls(controls);
    processor->getVoices().useLoadedVoice();editor->tick(1000);
    for(float scale:{.75f,1.f,1.25f,1.5f,1.75f,2.f,3.f,4.f})
    {
        const auto percent=juce::String(juce::roundToInt(scale*100));
        save(editor->getView().createComponentSnapshot(editor->getView().getLocalBounds(),true,scale),folder.getChildFile("ui-zoom-"+percent+".png"));
        for(bool old:{true,false})
        {
            juce::Image title(juce::Image::RGB,juce::roundToInt(760*scale),juce::roundToInt(32*scale),true);
            {
                juce::Graphics g(title);g.fillAll(phoqer::ui::win98::face);g.addTransform(juce::AffineTransform::scale(scale));
                if(old)phoqer::ui::legacyWindow98(g,{0,0,760,32},"PHOQER.EXE - KYRAX",phoqer::ui::paletteFor(3),true,22);
                else phoqer::ui::window98(g,{0,0,760,32},"PHOQER.EXE - KYRAX",phoqer::ui::paletteFor(3),true,22);
            }
            save(title,folder.getChildFile(juce::String("ui-title-")+(old?"old-":"fixed-")+percent+".png"));
        }
        save(selector<phoqer::ui::LegacyVoiceSlot98>(scale),folder.getChildFile("ui-selector-old-"+percent+".png"));
        save(selector<phoqer::ui::VoiceSlot98>(scale),folder.getChildFile("ui-selector-fixed-"+percent+".png"));
        // Independently examine glyph coverage: aligned pixel-font outlines
        // should not turn into interpolated intermediate-alpha bitmap pixels.
        const auto path=phoqer::ui::VoiceSlot98::letteringPath("SQUEAL",{0,0,62,22},scale);
        juce::Image mask(juce::Image::ARGB,juce::roundToInt(62*scale),juce::roundToInt(22*scale),true);
        {juce::Graphics ink(mask);ink.addTransform(juce::AffineTransform::scale(scale));ink.setColour(juce::Colours::white);ink.fillPath(path);}
        save(mask,folder.getChildFile("ui-selector-mask-"+percent+".png"));
    }
}
}
