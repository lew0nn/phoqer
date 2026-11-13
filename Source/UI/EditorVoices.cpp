// The voices: choosing one, VOICES.EXE, loading a voice file, entering and removing a key.
#include "EditorShared.h"

namespace phoqer::ui
{
void PhoqerView::chooseVoice(int index)
{
    history.performAsOne([&]{if(index==3)processor.getVoices().useLoadedVoice();else processor.getVoices().selectFreeVoice(index);});
    applyCharacter(processor.getVoices().getVisualVoice());refreshVoiceControls();
}
void PhoqerView::refreshVoiceControls()
{
    // A song's voice notice waits while any box is open (SAVE RECORDING, ENTER KEY...), then shows.
    if(dialog==nullptr)
        if(const auto notice=processor.getVoices().takeVoiceNotice();notice.isNotEmpty())
            showMessage("VOICE",juce::StringArray::fromTokens(notice,"|",""));
    const int active=processor.getVoices().getVisualVoice();
    const bool unlocked=processor.getVoices().isVoiceUnlocked();
    auto& tree=processor.getParameters().state;
    int mask=static_cast<int>(tree.getProperty("voiceQuickMask",15));
    if(active>=0&&active<4&&(mask&(1<<active))==0)     // the playing voice is always in the quick bar
    {
        mask|=1<<active;
        tree.setProperty("voiceQuickMask",mask,nullptr);
    }
    // The rest only changes with the voice, the browser, the quick bar, the voice file or the licence:
    // rebuild it when one of them has, not on every frame.
    const auto order=UiSettings::get().voiceOrder();
    const auto key=order+"|"+juce::String(active)+"|"+juce::String(browserVoice)+"|"+juce::String(mask)+"|"+juce::String(static_cast<int>(unlocked))
                  +"|"+juce::String(static_cast<int>(processor.getVoices().hasLoadedVoice()))+"|"+juce::String(static_cast<int>(processor.getVoices().isVoiceInstalled()))
                  +"|"+juce::String(licence::generation());
    if(key==voiceControlsKey)return;
    voiceControlsKey=key;
    // The quick bar: the voices in it from the left, in the order they were put in it; then the empty "+ ADD" slots.
    {
        int place=0;
        for(int pass=0;pass<2;++pass)
            for(const auto c:order)
            {
                const int i=c-'0';
                const bool shown=(i!=3||processor.getVoices().hasLoadedVoice())&&(mask&(1<<i))!=0;
                if(shown==(pass==0))voiceTabs[static_cast<size_t>(i)]->setBounds(466+place++*62,62,62,22);
            }
    }
    for(size_t i=0;i<4;++i)
    {
        const bool available=i!=3||processor.getVoices().hasLoadedVoice();     // a locked KYRAX leaves an empty "+ ADD" slot
        auto& tab=*voiceTabs[i];const bool shown=available&&(mask&(1<<i))!=0;
        if(tab.selected!=(active==static_cast<int>(i))||tab.available!=shown){tab.selected=active==static_cast<int>(i);tab.available=shown;tab.repaint();}
        voiceRows[i]->setVoiceState(browserVoice==static_cast<int>(i),active==static_cast<int>(i),available);
    }
    const auto status=licence::status();
    const bool installed=processor.getVoices().isVoiceInstalled();
    const juce::String keyState=status.key.kind==keys::Kind::gift?"GIFT TO "+juce::String(status.key.name).toUpperCase().substring(0,12)
                                                                :"LICENSED #"+juce::String(status.key.serial).paddedLeft('0',4);
    // Offline for a week or more, the row counts down the ticket's last days.
    const juce::String offline=status.daysLeft==1?"OFFLINE - 1 DAY LEFT":"OFFLINE - "+juce::String(status.daysLeft)+" DAYS LEFT";
    voiceRows[3]->setDetail(!installed?(status.unlocked?keyState+" - NOT INSTALLED":"NOT INSTALLED")
                           :status.paused?"PAUSED - GO ONLINE ONCE"
                           :!status.unlocked?(status.lockedElsewhere?"LOCKED ON THIS PC":"LOCKED - NEEDS A KEY")
                           :status.daysLeft<=7?offline:keyState);
    // 14 days offline: say it once per run, so a song that falls back to a free voice isn't a mystery.
    static bool pausedNoticeShown=false;
    if(status.ticketRanOut&&installed&&!pausedNoticeShown&&dialog==nullptr)
    {
        pausedNoticeShown=true;
        showMessage("KYRAX PAUSED",{"PHOQER HASN'T BEEN ONLINE FOR 14 DAYS.","~CONNECT ONCE AND KYRAX COMES BACK BY ITSELF"});
    }
    voiceRows[3]->setTooltip(licence::describe(status));
    const bool lockedKyrax=browserVoice==3&&!unlocked;
    const bool missingKyrax=browserVoice==3&&!installed;     // KYRAX always shows; without its file it offers LOAD VOICE
    tryOnlineButton.setVisible(lockedKyrax&&!missingKyrax);  // only a locked KYRAX has a button beside USE
    loadVoiceButton.setVisible(missingKyrax);
    loadVoiceButton.setBounds(lockedKyrax?juce::Rectangle<int>(10,247,104,22):juce::Rectangle<int>(10,247,218,22));
    useVoiceButton.setVisible(!(missingKyrax&&unlocked));
    useVoiceButton.setBounds(lockedKyrax?juce::Rectangle<int>(122,247,106,22):juce::Rectangle<int>(10,247,218,22));
    useVoiceButton.setButtonText(lockedKyrax?"ENTER KEY":"USE");
    keyButton.setVisible(browserVoice==3);
    keyButton.setButtonText(unlocked?"REMOVE KEY":"GET ON ITCH");
    quickVoice.setToggleState((mask&(1<<browserVoice))!=0,juce::dontSendNotification);
    quickVoice.setEnabled(!lockedKyrax&&browserVoice!=active);
    quickVoice.setTooltip(browserVoice==active?"THE PLAYING VOICE STAYS IN THE QUICK BAR":"");
}
bool PhoqerView::isInterestedInFileDrag(const juce::StringArray& files)
{
    for(const auto& f:files) if(f.endsWithIgnoreCase(".phqvoice")) return true;
    return false;
}
void PhoqerView::filesDropped(const juce::StringArray& files,int,int)
{
    for(const auto& f:files) if(f.endsWithIgnoreCase(".phqvoice")){installVoice(juce::File(f));return;}
}
void PhoqerView::chooseVoiceFile()
{
    chooser=std::make_unique<juce::FileChooser>("LOAD VOICE",juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("Downloads"),"*.phqvoice");
    chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
                         [safe=juce::Component::SafePointer<PhoqerView>(this)](const juce::FileChooser& fc)
                         {if(safe!=nullptr&&fc.getResult().existsAsFile())safe->installVoice(fc.getResult());});
}
void PhoqerView::installVoice(const juce::File& file)
{
    juce::String error;
    if(!processor.getVoices().installVoiceFile(file,error)){showMessage("VOICE",{error,"~PICK THE KYRAX.PHQVOICE FILE"});return;}
    browserVoice=3;voiceBrowser.setVisible(true);voiceBrowser.toFront(false);refreshVoiceControls();
    showMessage("VOICE",{"KYRAX IS INSTALLED.",processor.getVoices().isVoiceUnlocked()?"~IT'S READY TO PLAY":"~ENTER YOUR KEY TO PLAY IT"});
}
void PhoqerView::showVoiceBrowser()
{
    browserVoice=processor.getVoices().getVisualVoice();voiceBrowser.setVisible(true);voiceBrowser.toFront(false);refreshVoiceControls();
}
void PhoqerView::showEnterKey()
{
    if(dialog!=nullptr)return;
    Dialog98::Spec spec;
    spec.title="ENTER KEY";
    spec.character=character;
    spec.width=460.0f;
    spec.lines=licence::status().lockedElsewhere?juce::StringArray{"KYRAX IS LOCKED ON THIS COMPUTER.","~ENTER YOUR KEY TO UNLOCK IT HERE"}
                                                :juce::StringArray{"PASTE YOUR KYRAX KEY:","~FROM ITCH, OR A GIFT"};
    spec.fieldLabel="KEY:";
    spec.buttons={"UNLOCK","CANCEL"};
    spec.onClose=[this,safe=juce::Component::SafePointer<PhoqerView>(this)](int button,const juce::String& text)
    {
        if(button!=0)return;
        // The server answers in a moment: the box stays up with the key in it, then the answer replaces it.
        Dialog98::Spec checking;
        checking.title="ENTER KEY";
        checking.width=460.0f;
        checking.lines={"PASTE YOUR KYRAX KEY:","~CHECKING IT WITH LEWONN.COM..."};
        checking.fieldLabel="KEY:";
        checking.fieldText=text.trim();
        checking.buttons={"CANCEL"};
        checking.closable=false;
        checking.onClose=[this](int,const juce::String&){checkingKey=false;};
        showDialog(std::move(checking));
        checkingKey=true;
        processor.getVoices().online().run([safe,text]
        {
            int used=0;
            const auto error=licence::enter(text,&used);
            juce::MessageManager::callAsync([safe,error,used]{if(safe!=nullptr)safe->keyChecked(error,used);});
        });
    };
    showDialog(std::move(spec));
}
void PhoqerView::keyChecked(const juce::String& error,int used)
{
    processor.getVoices().refreshLicence();
    if(error.isEmpty()){browserVoice=3;applyCharacter(processor.getVoices().getVisualVoice());}
    refreshVoiceControls();
    if(!checkingKey)return;                                    // cancelled: the answer still counts, but quietly
    checkingKey=false;dialog=nullptr;
    if(error.isEmpty())
    {
        showMessage("KYRAX UNLOCKED",{licence::describe(licence::status()),"~ON "+juce::String(used)+" OF 3 COMPUTERS. KYRAX IS IN THE VOICE STRIP NOW"});
        return;
    }
    const juce::String hint=error.startsWith("THAT")?"~CHECK THAT THE WHOLE KEY WAS COPIED"
                           :error.contains("3 COMPUTERS")?"~REMOVE IT ON ONE OF THEM, OR WRITE TO KEYS@LEWONN.COM"
                           :error.contains("INTERNET")?"~CONNECT, THEN PRESS UNLOCK AGAIN"
                           :error.contains("BLOCKED")?"~IF THAT'S A MISTAKE, WRITE TO KEYS@LEWONN.COM":"";
    showMessage("ENTER KEY",{error,hint});
}
void PhoqerView::showRemoveKey()
{
    if(dialog!=nullptr)return;
    Dialog98::Spec spec;
    spec.title="REMOVE KEY";
    spec.character=character;
    spec.lines={"REMOVE THE KYRAX KEY FROM THIS COMPUTER?","~IT FREES ONE OF YOUR 3 SLOTS. ENTER THE KEY AGAIN ANY TIME"};
    spec.buttons={"REMOVE","CANCEL"};
    spec.onClose=[this,safe=juce::Component::SafePointer<PhoqerView>(this)](int button,const juce::String&)
    {
        if(button!=0)return;
        // Off this computer at once; the slot is freed on the server in the background.
        processor.getVoices().online().run([safe]
        {
            licence::deactivate();
            juce::MessageManager::callAsync([safe]{if(safe==nullptr)return;safe->processor.getVoices().refreshLicence();
                safe->applyCharacter(safe->processor.getVoices().getVisualVoice());safe->refreshVoiceControls();});
        });
    };
    showDialog(std::move(spec));
}
}
