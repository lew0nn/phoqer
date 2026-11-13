#pragma once

namespace playingBrightness {
using namespace phoqer::ui;
class Frame final : public juce::Component {
public:
    Frame(PhoqerView& parent,const juce::Image& portrait):view(parent),image(portrait){setSize(760,730);}
    void paint(juce::Graphics& g) override
    {
        view.paintEntireComponent(g,true);
        const auto area=windowClient({98,100,262,278}).withTrimmedBottom(18).reduced(3).toNearestInt();
        juce::Graphics::ScopedSaveState state(g);g.reduceClipRegion(area);g.setColour(juce::Colours::black);g.fillRect(area);
        g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);g.drawImage(image,{static_cast<float>(area.getX()),static_cast<float>(area.getY()),image.getWidth()*2.f,image.getHeight()*2.f});
        g.setColour(juce::Colours::black.withAlpha(.3f));for(int y=area.getY();y<area.getBottom();y+=2)g.fillRect(area.getX(),y,area.getWidth(),1);
    }
private:PhoqerView& view;juce::Image image;
};
inline void render(const juce::File& voice,const juce::File& folder)
{
    auto processor=std::make_unique<PhoqerAudioProcessor>();juce::String error;if(!processor->getVoices().loadVoiceFile(voice,error))throw std::runtime_error(error.toStdString());
    processor->prepareToPlay(44100,128);
    std::unique_ptr<juce::AudioProcessorEditor> base(processor->createEditor());auto* editor=dynamic_cast<PhoqerEditor*>(base.get());if(!editor)throw std::runtime_error("Missing editor");
    WindowControls controls;controls.audioSettings=controls.minimise=controls.toggleFullscreen=controls.close=[]{};editor->setWindowControls(controls);processor->getVoices().useLoadedVoice();editor->tick(1000);
    PortraitRenderer portrait;phoqer::ui::Expression expression{};expression.face.eyeOpen=.6f;
    identityStudies::save(portrait.render(3,expression,122,112),folder.getChildFile("ui-idle-source.png"));
    expression.face.mouthOpen=.7f;expression.face.mouthRound=.4f;expression.face.jawWidth=.7f;expression.face.headLift=.3f;expression.face.throatTension=.4f;
    for(int step=0;step<3;++step)
    {
        const float intensity=.5f+step*.25f;
        for(bool previous:{true,false})
        {
            // Intensity only enters the portrait's brightness expression.
            // Rescaling it recreates the previous20% lift with identical geometry.
            expression.face.intensity=previous?intensity*(.20f/.14f):intensity;
            const auto image=portrait.render(3,expression,122,112);Frame frame(editor->getView(),image);
            const auto name=juce::String(previous?"previous-":"current-")+juce::String(step+1);
            identityStudies::save(image,folder.getChildFile("ui-source-"+name+".png"));
            for(int scale:{1,2})identityStudies::save(frame.createComponentSnapshot(frame.getLocalBounds(),true,static_cast<float>(scale)),folder.getChildFile("ui-"+name+"-"+juce::String(scale)+"x.png"));
        }
    }
    // Capture the actual live editor driven by offline MIDI/audio telemetry.
    juce::AudioBuffer<float> block(2,128);
    for(int index=0;index<220;++index)
    {
        juce::MidiBuffer midi;if(index==0)midi.addEvent(juce::MidiMessage::noteOn(1,55,1.f),0);processor->processBlock(block,midi);
        if(index%6==0)editor->tick(1000.+index*128./44100.);
    }
    for(int scale:{1,2})identityStudies::save(editor->getView().createComponentSnapshot(editor->getView().getLocalBounds(),true,static_cast<float>(scale)),folder.getChildFile("ui-playing-"+juce::String(scale)+"x.png"));
}
}
