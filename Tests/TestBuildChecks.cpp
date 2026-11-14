#include "PluginProcessor.h"
#include "UI/Editor.h"
#include "UI/Settings.h"
#include "UI/Presets.h"
#include "Licence.h"
#include "ThirdParty/monocypher/monocypher-ed25519.h"
#include "RenderQuality.h"
#include "IdentityStudies.h"
#include "MiniVoiceStudies.h"
#include "PocketIconStudies.h"
#include "PlayingBrightness.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>
#include <map>
#include <set>

namespace {
constexpr int rate=44100,block=128;
constexpr double duration=8.4;
struct Note{double on,off;int offset;float velocity;};
const Note phrase[]={{.1,.55,0,.85f},{.85,1.3,7,.85f},{1.6,2.6,3,.85f},{3,3.8,0,.85f},{4.2,7.2,5,.85f}};
void require(bool result,const char* text){if(!result)throw std::runtime_error(text);}
void set(PhoqerAudioProcessor& p,const char* id,float value){auto* v=p.getParameters().getParameter(id);v->setValueNotifyingHost(v->convertTo0to1(value));}
void wav(const juce::File& file,const std::vector<float>& data){std::ofstream o(file.getFullPathName().toStdString(),std::ios::binary);auto u16=[&](uint16_t v){o.write(reinterpret_cast<char*>(&v),2);};auto u32=[&](uint32_t v){o.write(reinterpret_cast<char*>(&v),4);};const auto bytes=static_cast<uint32_t>(data.size()*4);o.write("RIFF",4);u32(bytes+36);o.write("WAVEfmt ",8);u32(16);u16(3);u16(2);u32(rate);u32(rate*8);u16(8);u16(32);o.write("data",4);u32(bytes);o.write(reinterpret_cast<const char*>(data.data()),bytes);}
std::vector<float> articulation(const phoqer::CharacterPreset* loaded,int character,const juce::String& kind,bool mutate=false){
    auto e=std::make_unique<phoqer::PhoqerEngine>();e->prepare(rate,block,2);
    phoqer::CharacterPreset snapshot=loaded?*loaded:phoqer::CharacterPreset{};
    phoqer::MacroState m;m.space=kind=="original"?.2f:0;m.character=static_cast<phoqer::SealCharacter>(character);m.loadedVoice=loaded?&snapshot:nullptr;
    std::vector<Note> notes;if(kind=="low"||kind=="original")notes.assign(std::begin(phrase),std::end(phrase));else if(kind=="hits"){int offsets[]={0,0,3,0,7,0,3,0};for(int i=0;i<8;++i)notes.push_back({.1+i*.4,.21+i*.4,offsets[i],.85f});}else notes.push_back({.1,7.2,0,.85f});
    phoqer::AudioBuffer b(2,block);std::vector<float> out;
    for(int index=0;index<static_cast<int>(std::ceil(rate*duration/block));++index){double t=static_cast<double>(index*block)/rate;
        if(kind=="stops")m.vowel=static_cast<float>(std::clamp(static_cast<int>((t-.1)/.75),0,8))/8;
        else if(kind=="hits"){float route[]={0,.125f,.375f,.5f,.75f,.875f,.25f,1};m.vowel=route[std::clamp(static_cast<int>((t+.1)/.4),0,7)];}
        else if(kind!="original"&&kind!="held")m.vowel=std::clamp(static_cast<float>((t-.35)/6.25),0.0f,1.0f);
        if(mutate&&t>1){snapshot.outputTrim=.01f;snapshot.formants[1].frequency=4000;}
        std::vector<phoqer::MidiEvent> events;auto add=[&](double seconds,phoqer::MidiEventType type,int note,float v){int sample=static_cast<int>(std::llround(seconds*rate));if(sample>=index*block&&sample<(index+1)*block)events.push_back({type,sample-index*block,0,note,v});};
        const int base=kind=="low"||kind=="hits"?43:55;for(auto n:notes){add(n.on,phoqer::MidiEventType::noteOn,base+n.offset,n.velocity);add(n.off,phoqer::MidiEventType::noteOff,base+n.offset,0);}
        std::stable_sort(events.begin(),events.end(),[](auto a,auto b){return a.sampleOffset<b.sampleOffset;});e->process(b,events.data(),static_cast<int>(events.size()),m,0);
        for(int i=0;i<block;++i)for(int c=0;c<2;++c)out.push_back(b.getReadPointer(c)[i]);
    }return out;
}
void snapshot(juce::Component& component,const juce::File& file){require(!file.existsAsFile(),"Snapshot output already exists");juce::FileOutputStream o(file);require(o.openedOk(),"Snapshot stream failed");require(juce::PNGImageFormat().writeImageToStream(component.createComponentSnapshot(component.getLocalBounds(),true,2),o),"Snapshot failed");juce::FileOutputStream one(file.getSiblingFile(file.getFileNameWithoutExtension()+"-1x.png"));require(juce::PNGImageFormat().writeImageToStream(component.createComponentSnapshot(component.getLocalBounds(),true,1),one),"Native1x snapshot failed");}
void renderViews(const juce::File& voice,const juce::File& folder){
    juce::String error;auto ui=std::make_unique<PhoqerAudioProcessor>();ui->getVoices().loadVoiceFile(voice,error);std::unique_ptr<juce::AudioProcessorEditor> base(ui->createEditor());auto* editor=dynamic_cast<phoqer::ui::PhoqerEditor*>(base.get());require(editor,"Editor type");
    phoqer::ui::WindowControls controls;controls.audioSettings=controls.minimise=controls.toggleFullscreen=controls.close=[]{};editor->setWindowControls(controls);
    for(int c=0;c<4;++c){if(c==3)ui->getVoices().useLoadedVoice();else ui->getVoices().selectFreeVoice(c);editor->tick(1000+c);if(c<3)require(!editor->getView().findChildWithID("yap")->isVisible()&&!editor->getView().findChildWithID("arp")->isVisible()&&!editor->getView().findChildWithID("nitro")->isVisible(),"YAP/ARP/NITRO shown outside KYRAX");require(editor->getView().findChildWithID("dice")->isVisible(),"the dice is missing");snapshot(editor->getView(),folder.getChildFile("ui-"+juce::String(c)+".png"));}
    {   // YAP + NITRO on KYRAX, then NITRO.EXE (right-click NITRO); a key tap adds a note
        auto& v=editor->getView();
        auto* yapB=dynamic_cast<phoqer::ui::IconToggle98*>(v.findChildWithID("yap"));auto* nitroB=dynamic_cast<phoqer::ui::IconToggle98*>(v.findChildWithID("nitro"));
        require(yapB!=nullptr&&nitroB!=nullptr&&yapB->isEnabled()&&nitroB->isEnabled(),"YAP and NITRO not on for KYRAX");
        yapB->setToggleState(true,juce::sendNotificationSync);nitroB->setToggleState(true,juce::sendNotificationSync);
        require(ui->getParameters().getRawParameterValue("nitro")->load()>0.5f&&ui->getParameters().getRawParameterValue("yap")->load()>0.5f,"YAP/NITRO buttons do not set their parameters");
        editor->tick(1010);snapshot(v,folder.getChildFile("ui-yap-nitro.png"));
        nitroB->onRightClick();auto* w=v.findChildWithID("nitroWindow");require(w!=nullptr&&w->isVisible(),"NITRO.EXE did not open");
        snapshot(v,folder.getChildFile("ui-nitro.png"));
        auto* keys=dynamic_cast<phoqer::ui::NitroKeys98*>(w->findChildWithID("nitroKeys"));require(keys!=nullptr,"NITRO keyboard missing");
        const auto tap=[&](int offset){const auto p=juce::Point<float>(static_cast<float>(keys->keyX(offset))+keys->whiteWidth()*0.5f,static_cast<float>(keys->getHeight())-3.0f);
            keys->mouseDown(juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,{},juce::MouseInputSource::defaultPressure,0,0,0,0,keys,keys,juce::Time::getCurrentTime(),p,juce::Time::getCurrentTime(),1,false));};
        tap(-12);require(juce::roundToInt(ui->getParameters().getRawParameterValue("nitroNote3")->load())==-12,"NITRO key tap did not add -12");
        snapshot(v,folder.getChildFile("ui-nitro-3.png"));
        tap(-12);require(juce::roundToInt(ui->getParameters().getRawParameterValue("nitroNote3")->load())==0,"NITRO key tap did not take -12 away");
        w->setVisible(false);yapB->setToggleState(false,juce::sendNotificationSync);nitroB->setToggleState(false,juce::sendNotificationSync);
        {   // ARP: on for KYRAX, its switch sets its parameter, a right-click opens ARP.EXE, RATE and HOLD buttons set theirs
            auto* arpB=dynamic_cast<phoqer::ui::IconToggle98*>(v.findChildWithID("arp"));require(arpB!=nullptr&&arpB->isEnabled()&&arpB->isVisible(),"ARP not on for KYRAX");
            arpB->setToggleState(true,juce::sendNotificationSync);require(ui->getParameters().getRawParameterValue("arp")->load()>0.5f,"ARP button does not set its parameter");
            arpB->onRightClick();auto* a=v.findChildWithID("arpWindow");require(a!=nullptr&&a->isVisible(),"ARP.EXE did not open");
            auto* rateButton=dynamic_cast<phoqer::ui::SegmentButton98*>(a->findChildWithID("arpRate1"));require(rateButton!=nullptr,"ARP RATE buttons missing");rateButton->onPick();
            require(juce::roundToInt(ui->getParameters().getRawParameterValue("arpRate")->load())==1&&rateButton->getToggleState(),"ARP RATE 1/8 did not take");
            auto* hold=dynamic_cast<phoqer::ui::SegmentButton98*>(a->findChildWithID("arpHoldOn"));hold->onPick();
            require(ui->getParameters().getRawParameterValue("arpHold")->load()>0.5f&&hold->getToggleState(),"ARP HOLD ON did not take");
            editor->tick(1011);snapshot(v,folder.getChildFile("ui-arp.png"));
            a->setVisible(false);arpB->setToggleState(false,juce::sendNotificationSync);dynamic_cast<phoqer::ui::SegmentButton98*>(a->findChildWithID("arpHoldOff"))->onPick();
            dynamic_cast<phoqer::ui::SegmentButton98*>(a->findChildWithID("arpRate2"))->onPick();
        }
    }
    {   // UPDATE.EXE: versions compared right, the version file read right, the notice, and each box (snapshots)
        using phoqer::updates::newer;
        require(newer("1.1.1","1.1.0")&&!newer("1.1.0","1.1.0")&&newer("1.10.0","1.9.9")&&!newer("1.0.9","1.1.0"),"Version compare wrong");
        const auto manifest=juce::JSON::parse(R"({"phoqer":{"version":"1.1.1","notes":["KYRAX ARP: new swing shapes","Fixed: dice on 4K screens","9.8 MB, about 10 seconds"],
            "windows":{"url":"https://github.com/lew0nn/phoqer/releases/download/v1.1.1/PHOQER-1.1.1-Setup.exe","sha256":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","size":10276044},
            "mac":{"page":"https://lewonn.com/phoqer"}},"kyrax":{"version":"1.1","page":"https://lewonn.itch.io/kyrax"}})");
        auto found=phoqer::updates::parse(manifest,false);
        require(found.reached&&found.phoqerNewer&&!found.kyraxNewer&&found.phoqer.notes[0]=="KYRAX ARP: NEW SWING SHAPES","Update file misread");
        require(!phoqer::updates::parse(juce::JSON::parse(R"({"phoqer":{"version":"1.1.1","windows":{"url":"ftp://x","sha256":"aa","size":1}}})"),false).phoqerNewer,"A bad update file was trusted");
        require(!phoqer::updates::parse(juce::JSON::parse(R"({"phoqer":{"version":"1.1.1","windows":{"url":"https://evil.example/PHOQER-1.1.1-Setup.exe",
            "sha256":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","size":10}}})"),false).phoqerNewer,"An update from another site was trusted");
        auto withKyrax=phoqer::updates::parse(manifest,true);require(withKyrax.kyraxNewer,"KYRAX update missed");
        auto& view=editor->getView();using Stage=phoqer::ui::PhoqerView::UpdateStage;
        view.showUpdate(Stage::prompt,withKyrax);require(view.updateNotice()=="2 UPDATES READY","Two-update notice wrong");
        view.showUpdate(Stage::prompt,found);require(view.updateNotice()=="UPDATE 1.1.1 READY"&&view.findChildWithID("updateWindow")->isVisible(),"Update notice or window missing");
        snapshot(view,folder.getChildFile("ui-update-prompt.png"));
        view.showUpdate(Stage::downloading,found,0.55f);snapshot(view,folder.getChildFile("ui-update-download.png"));
        view.showUpdate(Stage::ready,found);snapshot(view,folder.getChildFile("ui-update-ready.png"));
        view.showUpdate(Stage::none,phoqer::updates::parse(juce::var(),false));
        // a downloaded installer is checked: right size and SHA-256 kept, anything else deleted
        const auto installer=folder.getChildFile("fake-setup.exe");installer.replaceWithText("not really an installer");
        phoqer::updates::Release r;r.size=installer.getSize();r.sha256=juce::SHA256(installer).toHexString();
        require(phoqer::updates::verify(installer,r)&&installer.existsAsFile(),"A good download was refused");
        r.sha256=juce::String::repeatedString("0",64);require(!phoqer::updates::verify(installer,r)&&!installer.existsAsFile(),"A wrong download was kept");
        // the installer is told where each copy lives; a folder that isn't there (or has no PHOQER.vst3) is left out
        const auto app=folder.getChildFile("My App's folder"),plugs=folder.getChildFile("Plug Ins");
        app.createDirectory();plugs.getChildFile("PHOQER.vst3").createDirectory();
        const auto args=phoqer::updates::installArguments(app,plugs);
        require(args=="'/SP-','/SILENT','/DIR=\""+app.getFullPathName().replace("'","''")+"\"','/VST3DIR=\""+plugs.getFullPathName()+"\"'",(juce::String("Wrong installer arguments: ")+args).toRawUTF8());
        require(phoqer::updates::installArguments(folder.getChildFile("gone"),app)=="'/SP-','/SILENT'","A missing folder was passed to the installer");
    }
    editor->getView().showVoiceBrowser();snapshot(editor->getView(),folder.getChildFile("ui-browser.png"));
    auto* browser=editor->getView().findChildWithID("voiceBrowser");require(browser&&browser->isVisible(),"Voice browser not visible");
    require(browser->getWidth()==238&&browser->getHeight()==298,"Pocket list size changed");
    auto* burp=dynamic_cast<phoqer::ui::VoiceLibraryRow98*>(browser->findChildWithID("voiceRow0"));
    auto* kyrax=dynamic_cast<phoqer::ui::VoiceLibraryRow98*>(browser->findChildWithID("voiceRow3"));
    auto* quick=dynamic_cast<juce::ToggleButton*>(browser->findChildWithID("quickVoice"));
    auto* preview=dynamic_cast<juce::Button*>(browser->findChildWithID("tryOnline"));
    auto* use=dynamic_cast<juce::Button*>(browser->findChildWithID("useVoice"));
    require(burp&&kyrax&&quick&&preview&&use,"Pocket list controls missing");
    burp->onClick();require(ui->getVoices().getVisualVoice()==3&&burp->isSelectedVoice()&&kyrax->isActiveVoice(),"Browsing changed active voice");
    require(!preview->isVisible()&&use->getWidth()==218,"Free seals still show a preview button");
    snapshot(editor->getView(),folder.getChildFile("ui-browser-browse.png"));
    const int originalMask=static_cast<int>(ui->getParameters().state.getProperty("voiceQuickMask",15));
    quick->setToggleState(false,juce::dontSendNotification);quick->onClick();
    require(static_cast<int>(ui->getParameters().state.getProperty("voiceQuickMask"))==(originalMask&~1),"Quick bar changed another voice");
    snapshot(editor->getView(),folder.getChildFile("ui-browser-quick-off.png"));
    quick->setToggleState((originalMask&1)!=0,juce::dontSendNotification);quick->onClick();
    require(static_cast<int>(ui->getParameters().state.getProperty("voiceQuickMask"))==originalMask,"Quick bar restore failed");
    if((originalMask&1)!=0)     // BURP ticked again: it moved to the end of the quick bar, after the other voices shown
    {   require(phoqer::ui::UiSettings::get().voiceOrder().getLastCharacter()=='0',"Ticking a voice did not move it to the end");editor->tick(1500);
        auto* tab0=editor->getView().findChildWithID("voice0");auto* tab3=editor->getView().findChildWithID("voice3");
        require(tab0!=nullptr&&tab3!=nullptr&&tab0->getX()>tab3->getX(),"The quick bar did not follow its order");}
    use->onClick();require(ui->getVoices().getVisualVoice()==0&&!browser->isVisible(),"USE did not apply voice and close browser");
    editor->getView().showVoiceBrowser();require(burp->isSelectedVoice()&&burp->isActiveVoice(),"Browser did not reopen on active voice");
    snapshot(editor->getView(),folder.getChildFile("ui-browser-active-burp.png"));
    kyrax->onClick();use->onClick();require(ui->getVoices().getVisualVoice()==3&&!browser->isVisible(),"USE did not restore KYRAX");
    for(int v=0;v<4;++v){juce::FileOutputStream stream(folder.getChildFile("ui-badge-"+juce::String(v)+".png"));require(juce::PNGImageFormat().writeImageToStream(phoqer::ui::voiceLibraryBadges()[static_cast<size_t>(v)],stream),"Badge snapshot failed");}
    if(auto* metro=dynamic_cast<juce::Button*>(editor->getView().findChildWithID("metroOpen")))metro->onClick();snapshot(editor->getView(),folder.getChildFile("ui-neon.png"));
}
// WAIL BEND: WAIL clicked again opens the panel beside it; the slider and a typed value set the parameter;
// a click elsewhere closes it; and the processor plays the bend (12 ST sounds unlike 2.4 ST, 2.4 ST is the default).
std::vector<float> wailTake(float bend,bool hold=false,int seconds=2){
    auto p=std::make_unique<PhoqerAudioProcessor>();p->prepareToPlay(rate,block);p->getVoices().selectFreeVoice(0);set(*p,"behavior",3);
    if(bend>=0)set(*p,"wailBend",bend);if(hold)set(*p,"wailHold",1);
    juce::AudioBuffer<float> b(2,block);std::vector<float> out;
    for(int k=0;k<rate*seconds/block;++k){juce::MidiBuffer midi;if(k==0)midi.addEvent(juce::MidiMessage::noteOn(1,55,.85f),0);b.clear();p->processBlock(b,midi);for(int i=0;i<block;++i)out.push_back(b.getSample(0,i));}
    return out;
}
void wailBendChecks(const juce::File& folder){
    auto ui=std::make_unique<PhoqerAudioProcessor>();std::unique_ptr<juce::AudioProcessorEditor> base(ui->createEditor());auto* editor=dynamic_cast<phoqer::ui::PhoqerEditor*>(base.get());require(editor,"Editor type");
    auto& view=editor->getView();ui->getVoices().selectFreeVoice(0);editor->tick(3000);
    auto& bend=*ui->getParameters().getParameter("wailBend");
    require(std::abs(bend.convertFrom0to1(bend.getDefaultValue())-2.4f)<1e-4f,"WAIL BEND default is not 2.4");
    phoqer::ui::ChoiceButton* wail=nullptr;phoqer::ui::ChoiceButton* call=nullptr;
    for(auto* c:view.getChildren())if(auto* b=dynamic_cast<phoqer::ui::ChoiceButton*>(c)){if(b->getButtonText()=="WAIL")wail=b;if(b->getButtonText()=="CALL")call=b;}
    auto* panel=dynamic_cast<phoqer::ui::WailBendPanel98*>(view.findChildWithID("wailBend"));
    require(wail&&call&&panel,"WAIL BEND controls missing");
    require(!panel->isVisible(),"WAIL BEND shows before it is asked for");
    wail->onClick();require(wail->isSelected()&&!panel->isVisible(),"First WAIL click should only pick WAIL");
    wail->onClick();require(panel->isVisible()&&panel->getX()>=wail->getRight()&&panel->getY()>=wail->getY(),"Second WAIL click did not open the panel beside WAIL");
    panel->slider.setValue(12.0,juce::sendNotificationSync);
    require(std::abs(bend.convertFrom0to1(bend.getValue())-12.0f)<1e-4f&&panel->value.getText()=="12.0 ST","Slider did not set WAIL BEND");
    panel->value.setText("7.5",juce::sendNotificationSync);
    require(std::abs(bend.convertFrom0to1(bend.getValue())-7.5f)<1e-4f&&panel->value.getText()=="7.5 ST","Typed WAIL BEND not applied");
    panel->value.setText("99",juce::sendNotificationSync);
    require(std::abs(bend.convertFrom0to1(bend.getValue())-24.0f)<1e-4f,"Typed WAIL BEND not limited to 24");
    panel->slider.setValue(12.0,juce::sendNotificationSync);
    auto& hold=*ui->getParameters().getParameter("wailHold");
    require(hold.getValue()<.5f&&!panel->lock.getToggleState(),"WAIL HOLD is on by default");
    panel->lock.setToggleState(!panel->lock.getToggleState(),juce::sendNotificationSync);
    require(hold.getValue()>.5f&&panel->lock.getToggleState()&&panel->isVisible(),"Padlock did not switch WAIL HOLD on");
    require(panel->lock.getRight()<=panel->getWidth()&&panel->value.getRight()<panel->lock.getX(),"Padlock overlaps the value");
    snapshot(view,folder.getChildFile("ui-wail-bend.png"));
    panel->lock.setToggleState(!panel->lock.getToggleState(),juce::sendNotificationSync);
    require(hold.getValue()<.5f,"Padlock did not switch WAIL HOLD off");
    wail->onClick();require(!panel->isVisible(),"WAIL click did not close the panel");
    wail->onClick();require(panel->isVisible(),"Panel did not reopen");
    panel->close();call->onClick();require(!panel->isVisible()&&!wail->isSelected(),"Panel stayed open");
    const auto none=wailTake(-1),same=wailTake(2.4f),wide=wailTake(12.0f);
    require(none==same,"WAIL BEND 2.4 differs from the default");
    double diff=0,energy=0;for(size_t i=0;i<wide.size();++i){diff+=(wide[i]-same[i])*(wide[i]-same[i]);energy+=same[i]*same[i];}
    require(energy>0&&diff/energy>.5,"WAIL BEND 12 sounds like 2.4");
    const auto swing=wailTake(12.0f,false,4),held=wailTake(12.0f,true,4);     // 4 s each
    const size_t rise=static_cast<size_t>(rate)*2/block*block;           // the first second is the same rise either way
    require(std::equal(swing.begin(),swing.begin()+static_cast<long>(rate/2),held.begin()),"WAIL HOLD changed the rise");
    double d2=0,e2=0;require(swing.size()==held.size(),"WAIL takes differ in length");for(size_t i=rise;i<held.size();++i){d2+=(held[i]-swing[i])*(held[i]-swing[i]);e2+=swing[i]*swing[i];}
    require(e2>0&&d2/e2>.5,"WAIL HOLD sounds like the swing");
}
juce::String keyText(const char* name){return juce::File(juce::SystemStats::getEnvironmentVariable("PHOQER_TEST_KEYS",{})).getChildFile(name).loadFileAsString().trim();}
std::vector<uint8_t> unhex(const char* text){std::vector<uint8_t> out;for(size_t i=0;text[i]!=0&&text[i+1]!=0;i+=2)out.push_back(static_cast<uint8_t>(std::stoi(std::string(text+i,2),nullptr,16)));return out;}
juce::String keyText(const char* name);
// A stand-in for lewonn.com's activation server: up to 3 computers a key, tickets stamped with the TEST ticket key.
struct FakeServer{
    std::map<juce::String,std::set<juce::String>> slots;std::set<juce::String> banned;bool online=true;uint8_t secret[64]{};
    juce::String voiceText;int voicesSent=0;     // KYRAX's voice, sent with an unlock when the computer's copy is missing or different
    std::map<juce::String,juce::String> history;  // every KYRAX version, by fingerprint, for songs (POST /voice)
    struct Made{juce::String id,kind;int serial;juce::String name;};std::map<std::string,Made> made;   // the keys this server has made, by their random part
    FakeServer(){
        juce::MemoryOutputStream der;juce::Base64::convertFromBase64(der,juce::File(juce::SystemStats::getEnvironmentVariable("PHOQER_TEST_KEYS",{})).getChildFile("TEST-ticket-secret.txt").loadFileAsString().trim());
        require(der.getDataSize()>=32,"TEST-ticket-secret.txt missing");
        uint8_t seed[32],pub[32];const auto* d=static_cast<const uint8_t*>(der.getData())+der.getDataSize()-32;std::copy(d,d+32,seed);crypto_ed25519_key_pair(secret,pub,seed);
        auto add=[&](const char* file,Made m){made[*phoqer::keys::keyBody(keyText(file).toStdString())]=m;};
        add("k-buy.txt",{"1-B-137","B",137,""});add("k-gift.txt",{"1-G-1","G",1,"LEWONN"});add("k-blocked.txt",{"1-B-136","B",136,""});banned.insert("1-B-136");
    }
    const Made* find(const juce::String& key)const{const auto b=phoqer::keys::keyBody(key.toStdString());if(!b)return nullptr;const auto i=made.find(*b);return i==made.end()?nullptr:&i->second;}
    juce::String ticket(const juce::String& id,const juce::String& machine,juce::int64 expires){
        auto* o=new juce::DynamicObject;o->setProperty("k",id);o->setProperty("m",machine);o->setProperty("exp",expires);
        const auto payload=juce::JSON::toString(juce::var(o),true);juce::MemoryBlock message("PHQTICKET1|",11);message.append(payload.toRawUTF8(),payload.getNumBytesAsUTF8());
        uint8_t signature[64];crypto_ed25519_sign(signature,secret,static_cast<const uint8_t*>(message.getData()),message.getSize());
        return juce::Base64::toBase64(payload.toRawUTF8(),payload.getNumBytesAsUTF8())+"."+juce::Base64::toBase64(signature,64);}
    juce::var answer(const juce::String& path,const juce::var& body){
        if(!online)return {};
        auto* r=new juce::DynamicObject;const juce::var reply(r);const auto* key=find(body["key"].toString());const auto machine=body["machine"].toString();
        if(key==nullptr){r->setProperty("ok",false);r->setProperty("code","invalid");return reply;}const auto id=key->id;
        if(path=="/deactivate"){slots[id].erase(machine);r->setProperty("ok",true);return reply;}
        if(path=="/voice"){
            const auto found=history.find(body["sha256"].toString());
            if(banned.count(id)||!slots[id].count(machine)||found==history.end()){r->setProperty("ok",false);r->setProperty("code","locked");return reply;}
            auto* v=new juce::DynamicObject;v->setProperty("data",found->second);v->setProperty("sha256",found->first);r->setProperty("voice",juce::var(v));
            r->setProperty("ok",true);return reply;}
        if(banned.count(id)){r->setProperty("ok",false);r->setProperty("code","banned");return reply;}
        auto& used=slots[id];if(!used.count(machine)&&used.size()>=3){r->setProperty("ok",false);r->setProperty("code","full");return reply;}
        used.insert(machine);r->setProperty("ok",true);r->setProperty("used",static_cast<int>(used.size()));r->setProperty("max",3);
        r->setProperty("id",id);r->setProperty("kind",key->kind);r->setProperty("serial",key->serial);r->setProperty("name",key->name);
        r->setProperty("ticket",ticket(id,machine,juce::Time::currentTimeMillis()/1000+14*86400));
        const auto hash=juce::SHA256(voiceText.toRawUTF8(),voiceText.getNumBytesAsUTF8()).toHexString();
        if(body.hasProperty("phoqer")&&voiceText.isNotEmpty()&&body["voice"].toString()!=hash){
            auto* v=new juce::DynamicObject;v->setProperty("data",voiceText);v->setProperty("sha256",hash);v->setProperty("label","KYRAX TEST");r->setProperty("voice",juce::var(v));++voicesSent;}
        return reply;}
};
FakeServer& server(){static FakeServer s;return s;}
// The activation server's rules as PHOQER sees them: 3 computers, offline, blocked, the 14-day ticket and its renewal.
void activationChecks(){
    namespace licence=phoqer::licence;auto& S=server();licence::remove();S.slots.clear();S.banned={"1-B-136"};
    const auto buy=keyText("k-buy.txt");const auto now=juce::Time::currentTimeMillis()/1000;
    S.slots["1-B-137"]={"pc-a","pc-b","pc-c"};
    require(licence::enter(buy)=="THIS KEY IS ON 3 COMPUTERS ALREADY.","A 4th computer was let in");
    require(!licence::status().unlocked&&!licence::folder().getChildFile("KYRAX.unlock").exists(),"A refused key was stored");
    S.slots["1-B-137"].erase("pc-c");int used=0;
    require(licence::enter(buy,&used).isEmpty()&&used==3,"A freed slot could not be used");
    auto s=licence::status();require(s.unlocked&&s.daysLeft==licence::offlineDays,"A fresh ticket isn't 14 days");
    licence::deactivate();require(!licence::status().unlocked&&S.slots["1-B-137"].size()==2,"REMOVE KEY did not free the slot");
    S.online=false;require(licence::enter(buy)=="PHOQER NEEDS THE INTERNET ONCE TO UNLOCK KYRAX.","Offline unlock not explained");S.online=true;
    S.banned.insert("1-B-137");require(licence::enter(buy)=="THIS KEY HAS BEEN BLOCKED.","The server's block was ignored");S.banned.erase("1-B-137");
    require(licence::enter(buy).isEmpty(),"Unlock failed");
    const auto record=licence::folder().getChildFile("KYRAX.unlock");auto json=juce::JSON::parse(record.loadFileAsString());const auto me=json["ticketMachine"].toString();
    auto withTicket=[&](const juce::String& t){json.getDynamicObject()->setProperty("ticket",t);record.replaceWithText(juce::JSON::toString(json));licence::reload();return licence::status();};
    s=withTicket(S.ticket("1-B-137",me,now-60));require(!s.unlocked&&s.paused&&s.ticketRanOut,"A run-out ticket still plays");
    require(licence::describe(s)=="PAUSED - GO ONLINE ONCE","Paused line wrong");
    s=withTicket(S.ticket("1-B-137",me,now+6*86400-60));require(s.unlocked&&s.daysLeft==6,"The ticket's countdown is wrong");
    auto forged=S.ticket("1-B-137",me,now+365*86400);forged=forged.replaceSection(forged.indexOfChar('.')+3,1,forged[forged.indexOfChar('.')+3]=='A'?"B":"A");
    s=withTicket(forged);require(!s.unlocked,"A forged ticket was accepted");
    s=withTicket(S.ticket("1-B-137","another-computer",now+86400));require(!s.unlocked,"A ticket for another computer was accepted");
    s=withTicket(S.ticket("1-G-1",me,now+86400));require(!s.unlocked,"A ticket for another key was accepted");
    // Online, a ticket older than a day is renewed; offline it keeps counting down; a block removes it.
    s=withTicket(S.ticket("1-B-137",me,now+8*86400));
    S.online=false;{licence::Online online;licence::renewInBackground(online);}licence::reload();require(licence::status().daysLeft==8,"An offline renewal changed the ticket");
    S.online=true;{licence::Online online;licence::renewInBackground(online);}licence::reload();require(licence::status().daysLeft==licence::offlineDays,"The ticket was not renewed online");
    S.banned.insert("1-B-137");withTicket(S.ticket("1-B-137",me,now+8*86400));
    {licence::Online online;licence::renewInBackground(online);}licence::reload();s=licence::status();
    require(!s.unlocked&&s.paused&&!s.ticketRanOut,"A blocked key kept playing after going online");S.banned={"1-B-136"};
    // KYRAX's voice comes with the unlock: kept unreadable, not sent twice, fetched again when damaged, gone with REMOVE KEY.
    licence::remove();S.slots.clear();S.voicesSent=0;
    S.voiceText=R"({"format":"PHQVOICE","version":1,"id":"kyrax.test","name":"KYRAX","parameters":{"note":"a test voice, long enough"}})";
    require(licence::enter(buy).isEmpty()&&S.voicesSent==1,"The unlock didn't bring KYRAX's voice");
    require(licence::serverVoiceText()==S.voiceText,"The server's voice was not kept");
    const auto voiceCopy=licence::folder().getChildFile("Voices/KYRAX.voice");
    require(voiceCopy.existsAsFile()&&!voiceCopy.loadFileAsString().contains("PHQVOICE"),"The voice was stored readable");
    require(licence::enter(buy).isEmpty()&&S.voicesSent==1,"The voice was sent again to a computer that has it");
    {juce::MemoryBlock d;voiceCopy.loadFileAsData(d);static_cast<uint8_t*>(d.getData())[d.getSize()-10]^=0x20;voiceCopy.replaceWithData(d.getData(),d.getSize());}
    require(licence::serverVoiceText().isEmpty(),"A damaged voice copy was used");
    require(licence::enter(buy).isEmpty()&&S.voicesSent==2&&licence::serverVoiceText()==S.voiceText,"A damaged voice was not fetched again");
    voiceCopy.deleteFile();{licence::Online online;licence::renewInBackground(online);}
    require(licence::serverVoiceText()==S.voiceText&&S.voicesSent==3,"An unlocked computer without the voice didn't fetch it at start");
    licence::deactivate();require(!voiceCopy.exists(),"REMOVE KEY left the voice behind");
    S.voiceText.clear();licence::remove();S.slots.clear();
}
// Ed25519 as published (RFC 8032, test 1), then every kind of key the player can paste.
void keyChecks(){
    auto seed=unhex("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60");
    const auto pub=unhex("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
    const auto sig=unhex("e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b");
    uint8_t secret[64],derived[32],made[64];crypto_ed25519_key_pair(secret,derived,seed.data());
    require(std::equal(derived,derived+32,pub.begin()),"RFC 8032 public key mismatch");
    crypto_ed25519_sign(made,secret,nullptr,0);require(std::equal(made,made+64,sig.begin()),"RFC 8032 signature mismatch");
    require(crypto_ed25519_check(sig.data(),pub.data(),nullptr,0)==0,"RFC 8032 check failed");
    namespace licence=phoqer::licence;
    licence::remove();require(!licence::status().unlocked,"Remove left an unlock");
    require(licence::enter(keyText("k-fake.txt"))=="THAT IS NOT A VALID KYRAX KEY.","Fake key accepted");
    require(licence::enter("hello")=="THAT IS NOT A VALID KYRAX KEY.","Garbage accepted");
    require(licence::enter(keyText("k-blocked.txt"))=="THIS KEY HAS BEEN BLOCKED.","Blocked key accepted");
    // One character changed in the random part: a key the server never made.
    auto gift=keyText("k-gift.txt");gift=gift.replaceSection(gift.length()-1,1,gift.getLastCharacter()=='2'?"3":"2");
    require(licence::enter(gift)=="THAT IS NOT A VALID KYRAX KEY.","A changed key was accepted");
    require(!licence::status().unlocked&&!licence::folder().getChildFile("KYRAX.unlock").exists(),"Rejected key stored");
    require(licence::enter(" "+keyText("k-gift.txt")+"\n").isEmpty(),"Gift key refused");
    auto s=licence::status();require(s.unlocked&&s.key.kind==phoqer::keys::Kind::gift&&s.key.serial==1&&s.key.name=="LEWONN","Gift key misread");
    require(licence::describe(s)=="A GIFT FROM LEWONN TO LEWONN","Gift line wrong");
    licence::reload();require(licence::status().unlocked,"Unlock not remembered");
    // The same unlock record on another computer asks for the key again.
    const auto record=licence::folder().getChildFile("KYRAX.unlock");auto json=juce::JSON::parse(record.loadFileAsString());
    auto original=record.loadFileAsString();
    for(const auto* id:{"machine","device","name"})json.getDynamicObject()->setProperty(id,juce::String("another-")+id);   // another computer: every ID differs
    record.replaceWithText(juce::JSON::toString(json));
    licence::reload();s=licence::status();require(!s.unlocked&&s.lockedElsewhere,"Copied unlock worked on another computer");
    // The same computer with one ID changed (e.g. JUCE's device ID after an update) stays unlocked and is saved up to date.
    json=juce::JSON::parse(original);json.getDynamicObject()->setProperty("device","changed-by-an-update");record.replaceWithText(juce::JSON::toString(json));
    licence::reload();s=licence::status();require(s.unlocked,"One changed ID locked this computer");
    require(juce::JSON::parse(record.loadFileAsString())["device"].toString()!="changed-by-an-update","Changed ID not brought up to date");
    // An unlock from before the backup IDs (only "machine") still works here.
    json=juce::JSON::parse(original);json.getDynamicObject()->removeProperty("device");json.getDynamicObject()->removeProperty("name");json.getDynamicObject()->removeProperty("source");
    record.replaceWithText(juce::JSON::toString(json));licence::reload();require(licence::status().unlocked,"Older unlock file refused");
    json=juce::JSON::parse(original);for(const auto* id:{"machine","device","name"})json.getDynamicObject()->setProperty(id,juce::String("another-")+id);
    record.replaceWithText(juce::JSON::toString(json));licence::reload();s=licence::status();require(!s.unlocked&&s.lockedElsewhere,"Copied unlock worked on another computer");
    require(licence::enter(keyText("k-buy.txt")).isEmpty(),"Bought key refused");s=licence::status();
    require(s.unlocked&&s.key.kind==phoqer::keys::Kind::bought&&s.key.serial==137&&licence::describe(s)=="LICENSED - KEY #0137","Bought key misread");
    licence::remove();
}
// Locked: no KYRAX in the strip, TRY ONLINE / ENTER KEY / GET ON ITCH in VOICES.EXE, a song with KYRAX plays its free seal.
void lockedViews(const juce::MemoryBlock& kyraxSong,const juce::File& folder){
    phoqer::licence::remove();
    auto ui=std::make_unique<PhoqerAudioProcessor>();require(!ui->getVoices().isVoiceUnlocked()&&!ui->getVoices().hasLoadedVoice(),"KYRAX unlocked without a key");
    ui->getVoices().useLoadedVoice();require(ui->getVoices().getVisualVoice()!=3,"Locked KYRAX selectable");
    ui->prepareToPlay(rate,block);ui->getVoices().selectFreeVoice(0);
    ui->prepareToPlay(rate,block);
    std::unique_ptr<juce::AudioProcessorEditor> base(ui->createEditor());auto* editor=dynamic_cast<phoqer::ui::PhoqerEditor*>(base.get());require(editor,"Editor type");
    phoqer::ui::WindowControls controls;controls.audioSettings=controls.minimise=controls.toggleFullscreen=controls.close=[]{};editor->setWindowControls(controls);
    editor->tick(2000);require(!dynamic_cast<phoqer::ui::VoiceSlot98*>(editor->getView().findChildWithID("voice3"))->available,"Locked KYRAX shown in the voice strip");
    snapshot(editor->getView(),folder.getChildFile("ui-locked.png"));
    editor->getView().showVoiceBrowser();auto* browser=editor->getView().findChildWithID("voiceBrowser");
    dynamic_cast<juce::Button*>(browser->findChildWithID("voiceRow3"))->onClick();editor->tick(2001);
    auto* use=dynamic_cast<juce::Button*>(browser->findChildWithID("useVoice"));auto* preview=dynamic_cast<juce::Button*>(browser->findChildWithID("tryOnline"));auto* key=dynamic_cast<juce::Button*>(browser->findChildWithID("keyButton"));
    require(use->getButtonText()=="ENTER KEY"&&preview->getButtonText()=="TRY ONLINE"&&preview->isVisible()&&key->isVisible()&&key->getButtonText()=="GET ON ITCH","Locked buttons wrong");
    snapshot(editor->getView(),folder.getChildFile("ui-browser-locked.png"));
    use->onClick();snapshot(editor->getView(),folder.getChildFile("ui-enter-key.png"));
    base.reset();
    // A friend without KYRAX opens a song that uses it.
    auto song=std::make_unique<PhoqerAudioProcessor>();song->setStateInformation(kyraxSong.getData(),static_cast<int>(kyraxSong.getSize()));
    require(song->getVoices().getVisualVoice()!=3,"Locked song played KYRAX");
    juce::MemoryBlock resaved;song->getStateInformation(resaved);
    std::unique_ptr<juce::AudioProcessorEditor> songEditor(song->createEditor());auto* view=dynamic_cast<phoqer::ui::PhoqerEditor*>(songEditor.get());view->setWindowControls(controls);view->tick(2100);
    snapshot(view->getView(),folder.getChildFile("ui-song-without-kyrax.png"));
    songEditor.reset();
    // Unlocked with a gift: the browser shows who it is from.
    require(phoqer::licence::enter(keyText("k-gift.txt")).isEmpty(),"Unlock failed");
    auto back=std::make_unique<PhoqerAudioProcessor>();back->setStateInformation(resaved.getData(),static_cast<int>(resaved.getSize()));
    require(back->getVoices().getVisualVoice()==3,"Saving without KYRAX lost it");

    std::unique_ptr<juce::AudioProcessorEditor> giftEditor(back->createEditor());auto* gv=dynamic_cast<phoqer::ui::PhoqerEditor*>(giftEditor.get());gv->setWindowControls(controls);gv->tick(2200);
    gv->getView().showVoiceBrowser();gv->tick(2201);
    auto* giftKey=dynamic_cast<juce::Button*>(gv->getView().findChildWithID("voiceBrowser")->findChildWithID("keyButton"));require(giftKey->getButtonText()=="REMOVE KEY","Unlocked button wrong");
    snapshot(gv->getView(),folder.getChildFile("ui-browser-gift.png"));
    giftEditor.reset();
    phoqer::licence::remove();
}
}

int main(int argc,char** argv){try{
    juce::ScopedJuceInitialiser_GUI gui;
    // The checks press IN QUICK BAR, which moves voices in the quick bar's order; put the user's order back after.
    struct KeepOrder{juce::String order=phoqer::ui::UiSettings::get().voiceOrder();~KeepOrder(){phoqer::ui::UiSettings::get().setVoiceOrder(order);}} keepOrder;
    require(argc==3,"usage: PHOQERTestChecks <voice> <fresh-output>");
    const juce::File voice(juce::String::fromUTF8(argv[1])),folder(juce::String::fromUTF8(argv[2]));
    require(!folder.exists(),"Use a fresh output folder");folder.createDirectory();
    phoqer::VoiceData data;juce::String error;require(phoqer::VoiceData::read(voice,data,error),error.toRawUTF8());
    if(juce::SystemStats::getEnvironmentVariable("PHOQER_PLAYING_BRIGHTNESS",{}).isNotEmpty()){playingBrightness::render(voice,folder);std::cout<<"PLAYING_BRIGHTNESS_OK: representative comparison and live editor capture\n";return 0;}
    if(juce::SystemStats::getEnvironmentVariable("PHOQER_POCKET_BACKGROUNDS",{}).isNotEmpty()){pocketIconStudies::renderBackgrounds(voice,folder);std::cout<<"POCKET_BACKGROUNDS_OK: charcoal, violet, gray at1x and2x\n";return 0;}
    if(juce::SystemStats::getEnvironmentVariable("PHOQER_POCKET_ICONS",{}).isNotEmpty()){pocketIconStudies::render(voice,folder);std::cout<<"POCKET_ICONS_OK: README mascots and app badges at1x and2x\n";return 0;}
    if(juce::SystemStats::getEnvironmentVariable("PHOQER_MINI_VOICES",{}).isNotEmpty()){miniVoiceStudies::render(voice,folder);std::cout<<"MINI_VOICE_STUDIES_OK: five compact layouts at1x and2x\n";return 0;}
    if(juce::SystemStats::getEnvironmentVariable("PHOQER_APPROVED_GOLD",{}).isNotEmpty()){identityStudies::verifyApprovedGold(folder);std::cout<<"APPROVED_GOLD_OK: six native portrait sizes match Small lift exactly\n";return 0;}
    const auto goldStudy=juce::SystemStats::getEnvironmentVariable("PHOQER_GOLD_STUDY",{});
    if(goldStudy.isNotEmpty()){identityStudies::renderGold(voice,folder,goldStudy=="less");std::cout<<"GOLD_STUDY_OK:"<<goldStudy<<" full windows and closeups at1x and2x\n";return 0;}
    if(juce::SystemStats::getEnvironmentVariable("PHOQER_IDENTITY_STUDIES",{}).isNotEmpty()){identityStudies::render(voice,folder);std::cout<<"IDENTITY_STUDIES_OK: full windows and closeups at1x and2x\n";return 0;}
    if(juce::SystemStats::getEnvironmentVariable("PHOQER_RENDER_QUALITY",{}).isNotEmpty()){renderQuality::render(voice,folder);std::cout<<"RENDER_QUALITY_OK:75,100,125,150,175,200,300,400 percent\n";return 0;}
    if(juce::SystemStats::getEnvironmentVariable("PHOQER_UI_ONLY",{}).isNotEmpty()){renderViews(voice,folder);std::cout<<"UI_PREVIEW_OK: full windows at1x and2x\n";return 0;}
    require(juce::SystemStats::getEnvironmentVariable("PHOQER_LICENCE_DIR",{}).isNotEmpty()&&juce::SystemStats::getEnvironmentVariable("PHOQER_TEST_KEYS",{}).isNotEmpty(),"Set PHOQER_LICENCE_DIR and PHOQER_TEST_KEYS");
    // PHOQER_LIVE_UPDATE: the real update code against PHOQER_UPDATE_URL (the local test server): check, download, verify,
    // then schedule the installer for when this program closes (into this program's folder, then this program opens again).
    if(juce::SystemStats::getEnvironmentVariable("PHOQER_LIVE_UPDATE",{}).isNotEmpty()){
        const auto f=phoqer::updates::check(false);juce::String out;out<<"reached="<<(int)f.reached<<" version="<<f.phoqer.version<<" newer="<<(int)f.phoqerNewer;
        if(f.phoqerNewer){
            const auto file=folder.getChildFile("PHOQER-"+f.phoqer.version+"-Setup.exe");
            if(auto in=juce::URL(f.phoqer.url).createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress))){juce::FileOutputStream o(file);o.writeFromInputStream(*in,-1);}
            const bool ok=phoqer::updates::verify(file,f.phoqer);out<<" verified="<<(int)ok;
            if(ok)out<<" scheduled="<<(int)phoqer::updates::installWhenClosed(file,true,juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory(),{});
        }
        folder.getChildFile("live-update.txt").replaceWithText(out);return 0;}
    // PHOQER_LIVE_ACTIVATION: the real internet code against PHOQER_ACTIVATION_URL (the local test server), answers to live.txt.
    if(juce::SystemStats::getEnvironmentVariable("PHOQER_LIVE_ACTIVATION",{}).isNotEmpty()){
        int used=0;const auto first=phoqer::licence::enter(keyText("k-buy.txt"),&used);const auto s=phoqer::licence::status();
        folder.getChildFile("live.txt").replaceWithText("enter=["+first+"] used="+juce::String(used)+" unlocked="+juce::String((int)s.unlocked)+" days="+juce::String(s.daysLeft)
            +" fake=["+phoqer::licence::enter(keyText("k-fake.txt"))+"]"
            +" gift_after_8s=["+(juce::Thread::sleep(8000),phoqer::licence::enter(keyText("k-gift.txt")))+"]");return 0;}
    phoqer::licence::setTransportForTests([](const juce::String& path,const juce::var& body){return server().answer(path,body);});
    activationChecks();     // first: no PHOQER is open yet, so each renewal below gets a fresh worker
    require(VoiceSelection::bundledVoice().hash==data.hash,"Shipped KYRAX differs from the voice file");
    {   // LOAD VOICE: KYRAX is copied into PHOQER's own Voices folder and found there by a new instance; junk is refused
        auto target=VoiceSelection::installedVoiceFile();target.deleteFile();
        PhoqerAudioProcessor installer;juce::String installError;
        require(installer.getVoices().installVoiceFile(voice,installError)&&target.existsAsFile()&&installer.getVoices().isVoiceInstalled(),"Installing KYRAX failed");
        require(VoiceSelection::bundledVoice().hash==data.hash,"Installed KYRAX not found");
        const auto junk=folder.getChildFile("junk.phqvoice");junk.replaceWithText("not a voice");
        require(!installer.getVoices().installVoiceFile(junk,installError)&&installError.isNotEmpty(),"A broken voice file was accepted");
    }
    keyChecks();
    require(phoqer::licence::enter(keyText("k-buy.txt")).isEmpty(),"Unlock for checks failed");
    static_assert(static_cast<int>(phoqer::SealCharacter::count)==3);
    auto p=std::make_unique<PhoqerAudioProcessor>();
    auto* choice=dynamic_cast<juce::AudioParameterChoice*>(p->getParameters().getParameter("character"));require(choice&&choice->choices.size()==3,"Character choice changed");
    for(int i=0;i<3;++i)require(choice->convertFrom0to1(i*.5f)==static_cast<float>(i),"Automation mapping changed");
    require(p->getVoices().loadVoiceFile(voice,error),"Processor voice load failed");require(p->getVoices().getVisualVoice()==3,"Loaded selection missing");
    p->getParameters().state.setProperty("voiceQuickMask",11,nullptr);
    juce::MemoryBlock state;p->getStateInformation(state);
    folder.getChildFile("kyrax-state.bin").replaceWithData(state.getData(),state.getSize());
    auto recalled=std::make_unique<PhoqerAudioProcessor>();recalled->setStateInformation(state.getData(),static_cast<int>(state.getSize()));
    require(recalled->getVoices().getVisualVoice()==3&&recalled->getVoices().loadedVoiceHash()==p->getVoices().loadedVoiceHash(),"Voice recall failed");
    require(static_cast<int>(recalled->getParameters().state.getProperty("voiceQuickMask"))==11,"Quick bar recall failed");
    recalled->getVoices().selectFreeVoice(2);require(p->getVoices().getVisualVoice()==3&&recalled->getVoices().getVisualVoice()==2,"Instance isolation failed");
    // A song saves only which KYRAX version it uses, never the voice itself (shared projects carry none of it);
    // it keeps that version's sound when KYRAX changes: this computer keeps every version it played.
    auto saved=juce::JSON::parse(p->getVoices().captureVoiceSelection());require(!saved.hasProperty("voice")&&saved["hash"].toString()==data.hash,"A song carried the voice itself");
    auto changed=juce::JSON::parse(data.json);changed["parameters"].getDynamicObject()->setProperty("outputTrim",.5);
    const auto changedText=juce::JSON::toString(changed);phoqer::VoiceData changedData;require(phoqer::VoiceData::parse(changedText,changedData,error),"Changed voice invalid");
    phoqer::licence::keepVoice(changedText);saved.getDynamicObject()->setProperty("hash",changedData.hash);
    recalled->getVoices().restoreVoiceSelection(juce::JSON::toString(saved));
    require(recalled->getVoices().getVisualVoice()==3&&recalled->getVoices().loadedVoiceHash()==changedData.hash&&changedData.hash!=data.hash,"The song's own KYRAX version not used");
    {   // A version only the key server has (a song from another computer): a free seal plays until it arrives.
        auto other=juce::JSON::parse(changedText);other["parameters"].getDynamicObject()->setProperty("outputTrim",.6);
        const auto otherText=juce::JSON::toString(other);phoqer::VoiceData otherData;require(phoqer::VoiceData::parse(otherText,otherData,error),"Other voice invalid");
        server().history[otherData.hash]=otherText;auto fromElsewhere=saved;fromElsewhere.getDynamicObject()->setProperty("hash",otherData.hash);
        VoiceSelection::fetchSongVoices=false;recalled->getVoices().selectFreeVoice(2);
        recalled->getVoices().restoreVoiceSelection(juce::JSON::toString(fromElsewhere));
        require(recalled->getVoices().getVisualVoice()!=3&&recalled->getVoices().takeVoiceNotice().contains("GETTING IT"),"A missing KYRAX version didn't wait for the server");
        require(phoqer::licence::fetchVoice(otherData.hash)==otherText,"The song's KYRAX version was not fetched");
        recalled->getVoices().restoreVoiceSelection(juce::JSON::toString(fromElsewhere));
        require(recalled->getVoices().getVisualVoice()==3&&recalled->getVoices().loadedVoiceHash()==otherData.hash,"The fetched KYRAX version was not used");
        require(phoqer::licence::fetchVoice(juce::String::repeatedString("f",64)).isEmpty(),"An unknown KYRAX version was accepted");
        VoiceSelection::fetchSongVoices=true;
    }
    recalled->getVoices().selectFreeVoice(2);
    auto damaged=juce::JSON::parse(p->getVoices().captureVoiceSelection());damaged.getDynamicObject()->setProperty("voice","{\"format\":\"nope\"}");
    recalled->getVoices().restoreVoiceSelection(juce::JSON::toString(damaged));require(recalled->getVoices().getVisualVoice()==2&&recalled->getVoices().takeVoiceNotice().contains("DAMAGED"),"Damaged voice fallback failed");
    // Without a key: the free seal plays, the song keeps KYRAX until another voice is chosen.
    phoqer::licence::remove();const auto song=p->getVoices().captureVoiceSelection();recalled->getVoices().restoreVoiceSelection(song);
    require(recalled->getVoices().getVisualVoice()==2&&recalled->getVoices().takeVoiceNotice().contains("ISN'T UNLOCKED")&&recalled->getVoices().takeVoiceNotice().isEmpty(),"Locked fallback or one-time notice failed");
    require(recalled->getVoices().captureVoiceSelection()==song,"Locked song lost KYRAX when saved");
    recalled->getVoices().selectFreeVoice(1);require(!static_cast<bool>(juce::JSON::parse(recalled->getVoices().captureVoiceSelection())["selected"]),"Choosing a voice kept the locked one");
    require(phoqer::licence::enter(keyText("k-buy.txt")).isEmpty(),"Unlock failed");recalled->getVoices().restoreVoiceSelection(song);require(recalled->getVoices().getVisualVoice()==3,"Unlocked song did not play KYRAX");
    {   // A song opened while locked keeps its own voice when KYRAX is picked after the key goes in.
        auto changedSong=juce::JSON::parse(song);changedSong.getDynamicObject()->setProperty("voice",changedText);changedSong.getDynamicObject()->setProperty("hash",changedData.hash);
        phoqer::licence::remove();recalled->getVoices().refreshLicence();recalled->getVoices().restoreVoiceSelection(juce::JSON::toString(changedSong));recalled->getVoices().takeVoiceNotice();
        require(phoqer::licence::enter(keyText("k-buy.txt")).isEmpty(),"Unlock failed");recalled->getVoices().refreshLicence();recalled->getVoices().useLoadedVoice();
        require(recalled->getVoices().getVisualVoice()==3&&recalled->getVoices().loadedVoiceHash()==changedData.hash,"Picking KYRAX after unlocking replaced the song's own voice");
        // Removing the key while KYRAX plays: a free voice plays, and saving still keeps KYRAX.
        phoqer::licence::remove();recalled->getVoices().refreshLicence();
        require(recalled->getVoices().getVisualVoice()!=3&&static_cast<bool>(juce::JSON::parse(recalled->getVoices().captureVoiceSelection())["selected"]),"Removing the key lost the song's KYRAX");
        // Another program removing the unlock file is noticed.
        require(phoqer::licence::enter(keyText("k-buy.txt")).isEmpty(),"Unlock failed");
        const auto before=phoqer::licence::generation();
        phoqer::licence::folder().getChildFile("KYRAX.unlock").deleteFile();
        phoqer::licence::checkForChanges();
        require(phoqer::licence::generation()>before&&!phoqer::licence::status().unlocked,"A key removed elsewhere was not noticed");
        require(phoqer::licence::enter(keyText("k-buy.txt")).isEmpty(),"Unlock failed");recalled->getVoices().refreshLicence();
    }
    {   // Undo keeps a short token, not the voice file, and the token brings the same voice back.
        recalled->getVoices().useLoadedVoice();const auto token=recalled->getVoices().captureVoiceToken();const auto hash=recalled->getVoices().loadedVoiceHash();
        require(recalled->getVoices().getVisualVoice()==3&&token.length()<100,"Undo token holds the whole voice");
        recalled->getVoices().selectFreeVoice(0);recalled->getVoices().restoreVoiceToken(token);
        require(recalled->getVoices().getVisualVoice()==3&&recalled->getVoices().loadedVoiceHash()==hash,"Undo token did not restore the voice");
        recalled->getVoices().restoreVoiceToken("free");require(recalled->getVoices().getVisualVoice()!=3,"Undo token for a free voice failed");
    }
    {   // The host is told when the voice switches (it is not a parameter).
        struct Count final : juce::AudioProcessorListener
        {
            int n=0;
            void audioProcessorParameterChanged(juce::AudioProcessor*,int,float) override {}
            void audioProcessorChanged(juce::AudioProcessor*,const ChangeDetails& d) override { if(d.nonParameterStateChanged) ++n; }
        } count;
        recalled->addListener(&count);recalled->getVoices().useLoadedVoice();recalled->getVoices().selectFreeVoice(0);recalled->removeListener(&count);
        require(count.n>=2,"Voice switch not reported to the host");
    }
    {   // A KYRAX preset on a locked computer names its own free voice as the stand-in.
        recalled->getVoices().useLoadedVoice();
        juce::XmlElement x("PHOQERPRESET");x.setAttribute("name","LOCKED QA");x.setAttribute("loadedVoiceSelection",recalled->getVoices().captureVoiceSelection());
        auto* c=x.createNewChildElement("PARAM");c->setAttribute("id","character");c->setAttribute("value",2);
        const auto file=folder.getChildFile("locked.phqpreset");x.writeTo(file);
        phoqer::licence::remove();recalled->getVoices().refreshLicence();recalled->getVoices().selectFreeVoice(0);recalled->getVoices().takeVoiceNotice();
        phoqer::ui::PresetLibrary lockedLibrary(*recalled);
        require(lockedLibrary.loadFile(file)&&recalled->getVoices().takeVoiceNotice().contains("PLAYING GROAN INSTEAD"),"Locked preset named the wrong stand-in");
        lockedLibrary.refresh();require(lockedLibrary.find("KYX TALK SHOW")<0,"KYRAX presets listed on a locked computer");
        require(phoqer::licence::enter(keyText("k-buy.txt")).isEmpty(),"Unlock failed");recalled->getVoices().refreshLicence();
    }
    const auto invalid=folder.getChildFile("invalid.phqvoice");auto malformed=juce::JSON::parse(voice.loadFileAsString());malformed["parameters"].getDynamicObject()->setProperty("bassGain",1e20);invalid.replaceWithText(juce::JSON::toString(malformed));require(!recalled->getVoices().loadVoiceFile(invalid,error),"Unsafe voice data accepted");
    malformed=juce::JSON::parse(voice.loadFileAsString());malformed["parameters"].getDynamicObject()->setProperty("articulationMap",juce::Array<juce::var>{});invalid.replaceWithText(juce::JSON::toString(malformed));require(!recalled->getVoices().loadVoiceFile(invalid,error),"Bad map accepted");
    for(const auto* kind:{"stops","sweep","low","hits"})wav(folder.getChildFile(juce::String("kyrax-")+kind+".wav"),articulation(&data.preset,0,kind));
    for(int c=0;c<3;++c)wav(folder.getChildFile(juce::String("free-")+juce::String(c)+".wav"),articulation(nullptr,c,"original"));
    require(articulation(&data.preset,0,"held")==articulation(&data.preset,0,"held",true),"Loaded preset changed a held note");
    // Preset-file recall, then a released factory preset must return to its free voice.
    phoqer::ui::PresetLibrary library(*p);juce::XmlElement preset("PHOQERPRESET");preset.setAttribute("name","KYRAX QA");preset.setAttribute("loadedVoiceSelection",p->getVoices().captureVoiceSelection());auto* parameter=preset.createNewChildElement("PARAM");parameter->setAttribute("id","character");parameter->setAttribute("value",1);const auto presetFile=folder.getChildFile("kyrax.phqpreset");preset.writeTo(presetFile);p->getVoices().selectFreeVoice(0);require(library.loadFile(presetFile)&&p->getVoices().getVisualVoice()==3,"Paid preset recall failed");library.load(0);require(p->getVoices().getVisualVoice()==0,"Free preset did not clear loaded selection");
    {   // KYRAX's own presets, once unlocked: all five listed, and one sets KYRAX, YAP, NITRO and GLIDE, unchanged after loading.
        library.refresh();for(const auto* n:{"KYX TALK SHOW","KYX POWER STACK","KYX MEGA CHOIR","KYX HARP RISE","KYX DOUBLE TALK"})require(library.find(n)>0&&library.isBuiltIn(library.find(n)),"A KYRAX preset is missing");
        library.load(library.find("KYX DOUBLE TALK"));auto value=[&](const char* id){return p->getParameters().getRawParameterValue(id)->load();};
        require(p->getVoices().getVisualVoice()==3&&value("yap")>.5f&&value("nitro")>.5f&&juce::roundToInt(value("nitroNote1"))==12&&juce::roundToInt(value("glide"))==80,"KYX DOUBLE TALK did not load");
        require(!library.isDirty(),"A KYRAX preset shows as changed right after loading");
        library.load(library.find("KYX HARP RISE"));require(juce::roundToInt(value("nitroNote3"))==24&&juce::roundToInt(value("nitroStrum"))==80&&value("yap")<.5f&&juce::roundToInt(value("glide"))==0,"KYX HARP RISE did not load");
        library.load(0);require(p->getVoices().getVisualVoice()==0&&value("nitro")<.5f,"INIT after a KYRAX preset kept KYRAX");
    }
    // The processor adapter as well as the bare core, across the supported matrix.
    int cases=0;double maxPeak=0;
    for(double sampleRate:{44100.,48000.,88200.,96000.})for(int size:{32,64,128,512,1024})for(int mode=0;mode<5;++mode){
        auto processor=std::make_unique<PhoqerAudioProcessor>();require(processor->getVoices().loadVoiceFile(voice,error),"Matrix load failed");processor->prepareToPlay(sampleRate,size);set(*processor,"space",0);set(*processor,"behavior",static_cast<float>(mode));
        juce::AudioBuffer<float> b(2,size);double energy=0,tail=0;const int n=static_cast<int>(std::ceil(2.0*sampleRate/size));
        for(int k=0;k<n;++k){juce::MidiBuffer midi;if(k==0)midi.addEvent(juce::MidiMessage::noteOn(1,43,.85f),0);if(k==n/2)midi.addEvent(juce::MidiMessage::noteOff(1,43),0);processor->processBlock(b,midi);for(int c=0;c<2;++c)for(int i=0;i<size;++i){double x=b.getSample(c,i);require(std::isfinite(x),"Matrix non-finite");maxPeak=std::max(maxPeak,std::abs(x));energy+=x*x;if(k>n-5)tail+=x*x;}}
        require(energy>1e-7&&tail<.00001,"Matrix level or release failed");++cases;
    }
    require(maxPeak<.967,"Limiter ceiling exceeded");
    renderViews(voice,folder);
    wailBendChecks(folder);
    lockedViews(state,folder);
    const juce::String report="{\"result\":\"PASS\",\"matrix_cases\":"+juce::String(cases)+",\"max_peak\":"+juce::String(maxPeak,8)+",\"character_choices\":3,\"held_note_snapshot\":true,\"voice_state_and_preset_recall\":true,\"keys_rfc8032_fake_blocked_renamed_machine\":true,\"song_snapshot\":true,\"locked_fallback_kept_on_save\":true,\"try_online_link\":true,\"no_preview_buttons\":true,\"pocket_browser_interactions\":true,\"wail_bend_panel_and_sound\":true,\"wail_hold\":true,\"r20_fixes\":true,\"yap_nitro_ui\":true,\"arp_ui\":true,\"activation\":true,\"updates\":true,\"quick_bar_order\":true,\"ui_snapshots\":20}";
    folder.getChildFile("checks.json").replaceWithText(report);std::cout<<report<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
