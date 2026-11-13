#pragma once

// Offscreen design studies. These components are not used by the plugin editor.
namespace identityStudies {
using namespace phoqer::ui;

inline juce::Colour rgb(int value) { return juce::Colour(static_cast<juce::uint32>(value)|0xff000000); }
inline const char* names[]={"BURP","SQUEAL","GROAN","KYRAX"};
inline const char* descriptions[]={"LOW / ROUGH BARK","SHORT / BRIGHT YELPS","WARM / SLOW MOAN","GUTTURAL / WIDE VOWELS"};
inline const char* shortDescriptions[]={"LOW + ROUGH","BRIGHT YELPS","WARM MOAN","WIDE VOWELS"};

inline juce::Image portraitImage(const PortraitRenderer& renderer,int voice,int option,int width,int height)
{
    phoqer::ui::Expression expression{};expression.face.eyeOpen=.6f;
    if(voice!=3||option<0)return renderer.render(voice,expression,width,height);
    // Render the existing native portrait, then use the same luminance mapping
    // as KYRAX with three alternative highlight ramps.
    auto image=renderer.render(2,expression,width,height);
    const int ramps[3][5]={
        {0x100432,0x5a28dc,0xd06bc3,0xbea15e,0xe7cda0},
        {0x100432,0x5a28dc,0xd364cb,0xdc91c3,0xd9bc83},
        {0x100432,0x5a28dc,0xb86bdc,0xb29cdb,0xdad0ed}
    };
    const auto rampColour=[&](int stop)
    {
        if(option<3)return rgb(ramps[option][stop]);
        const auto muted=rgb(ramps[0][stop]);
        if(stop<3)return muted;
        // Gold refinements retain the chosen shadows and midtones. Options3-5
        // soften the gold; options6-8 return a small amount of its old warmth.
        if(option<6)return muted.interpolatedWith(rgb(ramps[1][stop]),.12f*(option-2));
        const int original[]={0x100432,0x5a28dc,0xec6ed2,0xfaec3c,0xffffd2};
        return muted.interpolatedWith(rgb(original[stop]),.08f*(option-5));
    };
    constexpr float stops[]={0,.30f,.55f,.80f,1};
    juce::Image::BitmapData data(image,juce::Image::BitmapData::readWrite);
    std::vector<float> luminance(static_cast<size_t>(width*height));
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){const auto c=data.getPixelColour(x,y);luminance[static_cast<size_t>(y*width+x)]=std::round(.299f*c.getRed()+.587f*c.getGreen()+.114f*c.getBlue())/255;}
    for(int y=0;y<height;++y)for(int x=0;x<width;++x)
    {
        float value=luminance[static_cast<size_t>(y*width+x)];
        if(y<height*80/450){float neighbours[81];int n=0;for(int yy=y-4;yy<=y+4;++yy)for(int xx=x-4;xx<=x+4;++xx)neighbours[n++]=luminance[static_cast<size_t>(juce::jlimit(0,height-1,yy)*width+juce::jlimit(0,width-1,xx))];std::nth_element(neighbours,neighbours+40,neighbours+81);if(value<neighbours[40]-.06f)value=neighbours[40];}
        int stop=0;while(stop<3&&value>stops[stop+1])++stop;
        const auto t=juce::jlimit(0.f,1.f,(value-stops[stop])/(stops[stop+1]-stops[stop]));
        data.setPixelColour(x,y,rampColour(stop).interpolatedWith(rampColour(stop+1),t));
    }
    return image;
}

inline void text(juce::Graphics& g,const juce::String& value,juce::Rectangle<int> area,float size=11,bool bold=false,juce::Colour ink=juce::Colours::black,juce::Justification align=juce::Justification::centredLeft)
{ drawText(g,value,area.toFloat(),pixelFont(size,bold),ink,align); }
inline void rule(juce::Graphics& g,int x,int y,int width)
{g.setColour(win98::shadow);g.fillRect(x,y,width,1);g.setColour(win98::light);g.fillRect(x,y+1,width,1);}
inline void push(juce::Graphics& g,juce::Rectangle<int> area,const juce::String& caption,bool primary=false)
{
    if(primary){g.setColour(juce::Colours::black);g.drawRect(area,1);area=area.reduced(1);}
    button98(g,area.toFloat(),false);text(g,caption,area,10,true,juce::Colours::black,juce::Justification::centred);
}
inline void checked(juce::Graphics& g,int x,int y,const juce::String& caption)
{
    sunken(g,{static_cast<float>(x),static_cast<float>(y+2),12,12},juce::Colours::white);
    g.setColour(juce::Colours::black);
    for(auto point:{juce::Point<int>(3,7),juce::Point<int>(4,8),juce::Point<int>(5,9),juce::Point<int>(6,7),juce::Point<int>(7,6),juce::Point<int>(8,5)})g.fillRect(x+point.x,y+point.y,2,2);
    text(g,caption,{x+18,y,150,16},9,true);
}
inline void tag(juce::Graphics& g,juce::Rectangle<int> area,const juce::String& caption,bool active=false)
{
    g.setColour(active?rgb(0x35204e):rgb(0xa5a5a5));g.fillRect(area);
    text(g,caption,area,8,true,active?rgb(0xe9d5fb):rgb(0x303030),juce::Justification::centred);
}
inline void framedPortrait(juce::Graphics& g,const juce::Image& image,juce::Rectangle<int> area)
{sunken(g,area.expanded(2).toFloat(),juce::Colours::black);g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);g.drawImageAt(image,area.getX(),area.getY());}

class Browser final : public juce::Component {
public:
    Browser(const PortraitRenderer& renderer,int option,int kind):portrait(renderer),palette(option),layout(kind)
    {setSize(620,layout==0?340:400);}
    void paint(juce::Graphics& g) override
    {
        g.fillAll(win98::face);bevel(g,getLocalBounds().toFloat(),true);
        auto bar=windowTitleBar(getLocalBounds().toFloat());
        juce::ColourGradient gradient(rgb(0x501edc),bar.getX(),0,rgb(0xd284c6),bar.getRight(),0,false);
        gradient.addColour(.6,rgb(0xa94cbb));g.setGradientFill(gradient);g.fillRect(bar);
        text(g,"VOICES.EXE",{8,3,300,16},11,true,juce::Colours::white);
        button98(g,{static_cast<float>(getWidth()-20),5,14,12},false);text(g,"X",{getWidth()-20,4,14,14},11,true,juce::Colours::black,juce::Justification::centred);
        if(layout==0)cards(g);else if(layout==1)list(g);else library(g);
    }
private:
    void card(juce::Graphics& g,int voice,juce::Rectangle<int> box,bool large)
    {
        g.setColour(voice==3?rgb(0x241731):rgb(0x17131e));g.fillRect(box);
        if(voice==3){g.setColour(rgb(0xa876d9));g.drawRect(box,2);}
        const int imageWidth=large?128:76,imageHeight=large?112:72;
        const auto image=portraitImage(portrait,voice,palette,imageWidth,imageHeight);
        g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
        g.drawImageAt(image,box.getX()+(box.getWidth()-imageWidth)/2,box.getY()+8);
        const int captionY=box.getY()+imageHeight+14;
        text(g,names[voice],{box.getX()+4,captionY,box.getWidth()-8,18},12,true,rgb(0xe3d1f2),juce::Justification::centred);
        if(large)text(g,shortDescriptions[voice],{box.getX()+4,captionY+20,box.getWidth()-8,14},8,false,rgb(0xb4a4c4),juce::Justification::centred);
        tag(g,{box.getX()+8,box.getBottom()-22,box.getWidth()-16,14},voice==3?"ACTIVE":"INCLUDED",voice==3);
    }
    void cards(juce::Graphics& g)
    {
        text(g,"YOUR VOICES",{14,28,260,20},13,true);text(g,"4 INSTALLED",{390,30,214,16},9,false,rgb(0x54505a),juce::Justification::centredRight);
        for(int voice=0;voice<4;++voice)card(g,voice,{14+voice*150,56,142,188},true);
        rule(g,14,256,592);
        text(g,"KYRAX",{18,268,170,20},13,true,rgb(0x5d288e));
        text(g,"GUTTURAL / WIDE VOWELS",{18,290,210,16},9);
        checked(g,244,268,"IN QUICK BAR");
        push(g,{414,266,192,28},"PREVIEW VOICE");
        push(g,{414,301,192,26},"USE VOICE",true);
        push(g,{244,299,158,28},"IMPORT VOICE...");
    }
    void list(juce::Graphics& g)
    {
        text(g,"YOUR VOICES",{14,28,260,20},13,true);
        sunken(g,{14,56,236,286},rgb(0x17131e));
        for(int voice=0;voice<4;++voice)
        {
            const int y=60+voice*69;const auto ink=voice==3?rgb(0xe3d1f2):rgb(0xb6a4c7);
            if(voice==3){g.setColour(rgb(0x35204e));g.fillRect(18,y,228,65);g.setColour(rgb(0xb286d8));g.fillRect(18,y,3,65);}
            const auto image=portraitImage(portrait,voice,palette,52,50);g.drawImageAt(image,26,y+7);
            text(g,names[voice],{88,y+12,148,19},12,true,ink);
            text(g,shortDescriptions[voice],{88,y+32,148,14},8,false,ink);
            if(voice==3)text(g,"ACTIVE",{88,y+47,148,12},8,true,rgb(0xe3d1f2));
        }
        text(g,"KYRAX",{274,28,180,22},18,true,rgb(0x5d288e));tag(g,{532,31,70,16},"ACTIVE",true);
        text(g,"GUTTURAL / WIDE VOWELS",{274,54,320,16},9);
        framedPortrait(g,portraitImage(portrait,3,palette,208,180),{330,84,208,180});
        text(g,"HEAVY BODY. MOVING MOUTH.",{274,280,320,16},10,true);
        checked(g,274,307,"IN QUICK BAR");
        rule(g,14,352,592);
        push(g,{14,365,236,24},"IMPORT VOICE...");push(g,{274,365,150,24},"PREVIEW");push(g,{436,365,166,24},"USE VOICE",true);
    }
    void library(juce::Graphics& g)
    {
        text(g,"VOICE LIBRARY",{14,28,300,20},13,true);
        push(g,{14,56,138,24},"INSTALLED  4",true);push(g,{160,56,138,24},"QUICK BAR  4");push(g,{458,56,148,24},"IMPORT...");
        for(int voice=0;voice<4;++voice)card(g,voice,{14+(voice%2)*150,94+(voice/2)*140,142,130},false);
        g.setColour(win98::shadow);g.fillRect(320,94,1,270);g.setColour(win98::light);g.fillRect(321,94,1,270);
        text(g,"KYRAX",{338,94,200,24},18,true,rgb(0x5d288e));tag(g,{538,98,66,16},"ACTIVE",true);
        text(g,"GUTTURAL / WIDE VOWELS",{338,124,266,16},9);
        framedPortrait(g,portraitImage(portrait,3,palette,180,154),{380,152,180,154});
        checked(g,338,322,"IN QUICK BAR");push(g,{338,350,128,30},"PREVIEW");push(g,{476,350,128,30},"USE VOICE",true);
    }
    const PortraitRenderer& portrait;int palette,layout;
};

class Stage final : public juce::Component {
public:
    Stage(PhoqerView& existing,int option,int layout):view(existing),palette(option)
    {
        setSize(760,730);
        const auto area=windowClient({98,100,262,278}).withTrimmedBottom(18).reduced(3).toNearestInt();
        portraitArea=area;image=portraitImage(portrait,3,palette,area.getWidth()/2,area.getHeight()/2);
        if(layout>=0){browser=std::make_unique<Browser>(portrait,palette,layout);addAndMakeVisible(*browser);browser->setTopLeftPosition((getWidth()-browser->getWidth())/2,154);}
    }
    void paint(juce::Graphics& g) override
    {
        view.paintEntireComponent(g,true);
        juce::Graphics::ScopedSaveState state(g);g.reduceClipRegion(portraitArea);
        g.setColour(juce::Colours::black);g.fillRect(portraitArea);
        g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
        g.drawImage(image,{static_cast<float>(portraitArea.getX()),static_cast<float>(portraitArea.getY()),image.getWidth()*2.f,image.getHeight()*2.f});
        g.setColour(juce::Colours::black.withAlpha(.3f));for(int y=portraitArea.getY();y<portraitArea.getBottom();y+=2)g.fillRect(portraitArea.getX(),y,portraitArea.getWidth(),1);
    }
    Browser* panel() const {return browser.get();}
    const juce::Image& nativePortrait() const {return image;}
private:
    PhoqerView& view;int palette;PortraitRenderer portrait;juce::Image image;juce::Rectangle<int> portraitArea;std::unique_ptr<Browser> browser;
};

inline void save(const juce::Image& image,const juce::File& file)
{if(file.exists())throw std::runtime_error("Fresh study output required");juce::FileOutputStream stream(file);if(!stream.openedOk()||!juce::PNGImageFormat().writeImageToStream(image,stream))throw std::runtime_error("Study PNG failed");}
inline void render(const juce::File& voice,const juce::File& folder)
{
    auto processor=std::make_unique<PhoqerAudioProcessor>();juce::String error;
    if(!processor->getVoices().loadVoiceFile(voice,error))throw std::runtime_error(error.toStdString());
    std::unique_ptr<juce::AudioProcessorEditor> base(processor->createEditor());auto* editor=dynamic_cast<PhoqerEditor*>(base.get());if(!editor)throw std::runtime_error("Missing editor");
    WindowControls controls;controls.audioSettings=controls.minimise=controls.toggleFullscreen=controls.close=[]{};editor->setWindowControls(controls);processor->getVoices().useLoadedVoice();editor->tick(1000);
    for(int palette=-1;palette<3;++palette)
    {
        const auto letter=palette<0?juce::String("current"):juce::String::charToString(static_cast<juce::juce_wchar>('a'+palette));
        Stage stage(editor->getView(),palette,-1);
        for(int scale:{1,2})save(stage.createComponentSnapshot(stage.getLocalBounds(),true,static_cast<float>(scale)),folder.getChildFile("palette-"+letter+"-"+juce::String(scale)+"x.png"));
        save(stage.nativePortrait(),folder.getChildFile("portrait-"+letter+".png"));
        if(palette<0)continue;
        for(int layout=0;layout<3;++layout)
        {
            Stage browser(editor->getView(),palette,layout);const auto type=juce::String::charToString(static_cast<juce::juce_wchar>('a'+layout));
            for(int scale:{1,2})
            {
                save(browser.createComponentSnapshot(browser.getLocalBounds(),true,static_cast<float>(scale)),folder.getChildFile("panel-"+type+"-palette-"+letter+"-"+juce::String(scale)+"x.png"));
                save(browser.panel()->createComponentSnapshot(browser.panel()->getLocalBounds(),true,static_cast<float>(scale)),folder.getChildFile("closeup-"+type+"-palette-"+letter+"-"+juce::String(scale)+"x.png"));
            }
        }
    }
}
inline void renderGold(const juce::File& voice,const juce::File& folder,bool less)
{
    auto processor=std::make_unique<PhoqerAudioProcessor>();juce::String error;
    if(!processor->getVoices().loadVoiceFile(voice,error))throw std::runtime_error(error.toStdString());
    std::unique_ptr<juce::AudioProcessorEditor> base(processor->createEditor());auto* editor=dynamic_cast<PhoqerEditor*>(base.get());if(!editor)throw std::runtime_error("Missing editor");
    WindowControls controls;controls.audioSettings=controls.minimise=controls.toggleFullscreen=controls.close=[]{};editor->setWindowControls(controls);processor->getVoices().useLoadedVoice();editor->tick(1000);
    const auto crop=windowClient({98,100,262,278}).withTrimmedBottom(18).reduced(3).toNearestInt();
    for(int step=0;step<4;++step)
    {
        const int option=step==0?0:(less?step+2:step+5);
        Stage stage(editor->getView(),option,-1);
        for(int scale:{1,2})
        {
            const auto suffix=juce::String(step)+"-"+juce::String(scale)+"x.png";
            save(stage.createComponentSnapshot(stage.getLocalBounds(),true,static_cast<float>(scale)),folder.getChildFile("gold-"+suffix));
            save(stage.createComponentSnapshot(crop,true,static_cast<float>(scale)),folder.getChildFile("portrait-"+suffix));
        }
        save(stage.nativePortrait(),folder.getChildFile("source-"+juce::String(step)+".png"));
    }
    // Show the middle refinement in each unchosen library layout for context.
    for(int layout=0;layout<3;++layout)
    {
        Stage stage(editor->getView(),less?4:7,layout);
        const auto letter=juce::String::charToString(static_cast<juce::juce_wchar>('a'+layout));
        for(int scale:{1,2})
        {
            const auto suffix=letter+"-"+juce::String(scale)+"x.png";
            save(stage.createComponentSnapshot(stage.getLocalBounds(),true,static_cast<float>(scale)),folder.getChildFile("panel-"+suffix));
            save(stage.panel()->createComponentSnapshot(stage.panel()->getLocalBounds(),true,static_cast<float>(scale)),folder.getChildFile("closeup-"+suffix));
        }
    }
}
inline void verifyApprovedGold(const juce::File& folder)
{
    PortraitRenderer renderer;phoqer::ui::Expression expression{};expression.face.eyeOpen=.6f;
    for(auto size:{juce::Point<int>(122,112),juce::Point<int>(144,124),juce::Point<int>(128,112),juce::Point<int>(208,180),juce::Point<int>(180,154),juce::Point<int>(52,50)})
    {
        const auto actual=renderer.render(3,expression,size.x,size.y);
        const auto expected=portraitImage(renderer,3,7,size.x,size.y);
        juce::Image::BitmapData a(actual,juce::Image::BitmapData::readOnly),b(expected,juce::Image::BitmapData::readOnly);
        for(int y=0;y<size.y;++y)for(int x=0;x<size.x;++x)if(a.getPixelColour(x,y)!=b.getPixelColour(x,y))throw std::runtime_error("Approved gold pixel mismatch");
        save(actual,folder.getChildFile("approved-"+juce::String(size.x)+"x"+juce::String(size.y)+".png"));
    }
}
}
