#include "VoiceSelection.h"
#include "Licence.h"

VoiceSelection::VoiceSelection(juce::AudioProcessor& h, juce::AudioProcessorValueTreeState& s)
    : host(h), state(s), character(*s.getRawParameterValue("character"))
{
    setVoiceData(bundledVoice());
    refreshLicence();
    phoqer::licence::renewInBackground(licenceOnline);     // renews the 14-day ticket when it's a day old, if online
    loadedSelected.store(false);
    playing = loadedData.preset;
    voicePending = false;
}

const phoqer::CharacterPreset* VoiceSelection::audioVoice() noexcept
{
    if (voiceLock.tryEnter())        // never waits: a voice being loaded is picked up next block
    {
        if (voicePending) { playing = loadedData.preset; voicePending = false; }
        voiceLock.exit();
    }
    return loadedSelected.load() && voiceAvailable.load() && voiceUnlocked.load() ? &playing : nullptr;
}

// The voice file beside the standalone app, or in the VST3 bundle's Resources.
juce::File VoiceSelection::bundledVoiceFile()
{
    auto folder=juce::File::getSpecialLocation(juce::File::currentApplicationFile).getParentDirectory();
    for(int level=0;level<3;++level)     // the app folder, or up to the VST3 bundle's Contents; never further up the disk
    {
        for(const auto* relative:{"Voices/KYRAX.phqvoice","Resources/Voices/KYRAX.phqvoice"})
        {
            const auto file=folder.getChildFile(relative);
            if(file.existsAsFile()) return file;
        }
        folder=folder.getParentDirectory();
    }
    return {};
}

// PHOQER's own copy, made by LOAD VOICE; it sits beside the licence, so the standalone and the VST3 share it.
juce::File VoiceSelection::installedVoiceFile()
{
    return phoqer::licence::folder().getChildFile("Voices/KYRAX.phqvoice");
}

// KYRAX's voice: the copy the activation server sent this computer first, then one added with LOAD VOICE, then a test
// build's bundled file.
phoqer::VoiceData VoiceSelection::bundledVoice()
{
    phoqer::VoiceData data;
    juce::String error;
    const auto fromServer=phoqer::licence::serverVoiceText();
    if(fromServer.isNotEmpty()&&phoqer::VoiceData::parse(fromServer,data,error)&&data.name=="KYRAX") return data;
    data={};
    if(!phoqer::VoiceData::read(installedVoiceFile(),data,error)) phoqer::VoiceData::read(bundledVoiceFile(),data,error);
    return data;
}

bool VoiceSelection::installVoiceFile(const juce::File& file,juce::String& error)
{
    phoqer::VoiceData data;
    if(!phoqer::VoiceData::read(file,data,error)){error="THAT FILE IS NOT A PHOQER VOICE.";return false;}
    if(data.name!="KYRAX"){error="THAT IS NOT THE KYRAX VOICE.";return false;}
    const auto target=installedVoiceFile();
    if(file!=target&&!(target.getParentDirectory().createDirectory()&&file.copyFileTo(target))){error="COULD NOT COPY THE VOICE INTO PHOQER.";return false;}
    setVoiceData(std::move(data));
    return true;
}

void VoiceSelection::setVoiceData(phoqer::VoiceData data)
{
    if(data.json.isEmpty()) return;     // no voice file installed
    const juce::SpinLock::ScopedLockType guard(voiceLock);
    loadedData=std::move(data);
    voicePending=true;
    voiceAvailable.store(true);
}

bool VoiceSelection::loadVoiceFile(const juce::File& file,juce::String& error)
{
    phoqer::VoiceData data;
    if(!phoqer::VoiceData::read(file,data,error)) return false;
    setVoiceData(std::move(data));
    {const juce::SpinLock::ScopedLockType guard(voiceLock);keptSelection.clear();loadNotice.clear();}
    loadedSelected.store(voiceUnlocked.load());
    notifyVoiceChange();
    return true;
}

void VoiceSelection::refreshLicence()
{
    const bool was=voiceUnlocked.load();
    const bool now=phoqer::licence::status().unlocked;
    if(was&&!now&&loadedSelected.load())
    {
        // The key went away under a song that uses KYRAX: it plays a free voice now, but keeps its
        // choice, so saving it here does not lose KYRAX for a computer that has the key.
        const auto selection=captureVoiceSelection();
        const juce::SpinLock::ScopedLockType guard(voiceLock);
        keptSelection=selection;
    }
    voiceUnlocked.store(now);
    // The first unlock brings KYRAX's voice with it: take it now. A newer voice later waits for the next start, so a
    // voice never changes under a song that is playing.
    if(now&&!voiceAvailable.load()) setVoiceData(bundledVoice());
    if(!now) loadedSelected.store(false);
    if(was!=now) notifyVoiceChange();
}

void VoiceSelection::LicenceWatch::timerCallback()
{
    phoqer::licence::checkForChanges();
    const auto now=phoqer::licence::generation();
    if(now==seen) return;
    seen=now;
    owner.refreshLicence();
}

void VoiceSelection::notifyVoiceChange()
{
    // The voice is not a parameter, so tell the host the state changed: it marks the project as modified.
    host.updateHostDisplay(juce::AudioProcessorListener::ChangeDetails{}.withNonParameterStateChanged(true));
}

void VoiceSelection::useLoadedVoice()
{
    if(!voiceUnlocked.load()) return;
    juce::String kept;
    {const juce::SpinLock::ScopedLockType guard(voiceLock);kept=keptSelection;}
    if(kept.isNotEmpty())
    {
        // A song that was opened where KYRAX was locked keeps the voice it was saved with: an update
        // to the installed file never changes a saved sound.
        restoreVoiceSelection(kept);
        if(loadedSelected.load()){notifyVoiceChange();return;}
    }
    if(!voiceAvailable.load()) return;
    {const juce::SpinLock::ScopedLockType guard(voiceLock);keptSelection.clear();}
    loadedSelected.store(true);
    notifyVoiceChange();
}

void VoiceSelection::selectFreeVoice(int index)
{
    const bool wasLoaded=loadedSelected.load();
    {const juce::SpinLock::ScopedLockType guard(voiceLock);keptSelection.clear();}
    loadedSelected.store(false);
    if(wasLoaded) notifyVoiceChange();
    auto* parameter=state.getParameter("character");
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(juce::jlimit(0,2,index))));
    parameter->endChangeGesture();
}

juce::String VoiceSelection::loadedVoiceName() const
{ const juce::SpinLock::ScopedLockType guard(voiceLock);return loadedData.name; }
juce::String VoiceSelection::loadedVoiceHash() const
{ const juce::SpinLock::ScopedLockType guard(voiceLock);return loadedData.hash; }
juce::String VoiceSelection::takeVoiceNotice()
{ const juce::SpinLock::ScopedLockType guard(voiceLock);auto notice=loadNotice;loadNotice.clear();return notice; }

juce::String VoiceSelection::captureVoiceSelection() const
{
    const juce::SpinLock::ScopedLockType guard(voiceLock);
    if(keptSelection.isNotEmpty()&&!loadedSelected.load()) return keptSelection;
    auto* object=new juce::DynamicObject;
    object->setProperty("selected",loadedSelected.load());
    if(loadedSelected.load())
    {
        object->setProperty("id",loadedData.id);
        object->setProperty("name",loadedData.name);
        object->setProperty("hash",loadedData.hash);
        object->setProperty("voice",loadedData.json);
    }
    return juce::JSON::toString(juce::var(object),true);
}

// Undo keeps a short token per step instead of the whole selection (a voice file can be 64 KB);
// each distinct selection is stored once here, on the message thread.
juce::String VoiceSelection::captureVoiceToken() const
{
    juce::String token;
    {
        const juce::SpinLock::ScopedLockType guard(voiceLock);
        if(keptSelection.isNotEmpty()&&!loadedSelected.load()) token="kept:"+juce::String::toHexString(keptSelection.hashCode64());
        else if(loadedSelected.load()) token="voice:"+loadedData.hash;
        else return "free";
    }
    if(voiceTokens.find(token)==voiceTokens.end()) voiceTokens[token]=captureVoiceSelection();
    return token;
}

void VoiceSelection::restoreVoiceToken(const juce::String& token)
{
    const auto found=voiceTokens.find(token);
    restoreVoiceSelection(found!=voiceTokens.end()?found->second:juce::String("{\"selected\":false}"));
}

void VoiceSelection::restoreVoiceSelection(const juce::String& selection)
{
    const auto value=juce::JSON::parse(selection);
    {const juce::SpinLock::ScopedLockType guard(voiceLock);keptSelection.clear();loadNotice.clear();}
    loadedSelected.store(false);
    if(!static_cast<bool>(value["selected"])) return;
    refreshLicence();
    const char* freeNames[]{"BURP","SQUEAL","GROAN"};
    const juce::String standIn=freeNames[juce::jlimit(0,2,static_cast<int>(std::lround(character.load())))];
    phoqer::VoiceData data;
    juce::String error;
    const auto text=value["voice"].toString();
    if(text.isEmpty()) data=bundledVoice();       // older test sessions saved only a file path
    else if(!phoqer::VoiceData::parse(text,data,error))
    {
        const juce::SpinLock::ScopedLockType guard(voiceLock);
        loadNotice="THIS SONG'S VOICE DATA IS DAMAGED.|~PLAYING "+standIn+" INSTEAD.";
        return;
    }
    // The name the song saved, for the messages (an older test session may have none).
    auto savedName=value["name"].toString();
    if(savedName.isEmpty()) savedName=data.name.isNotEmpty()?data.name:juce::String("KYRAX");
    if(!voiceUnlocked.load()||data.name!="KYRAX")
    {
        const juce::SpinLock::ScopedLockType guard(voiceLock);
        keptSelection=selection;
        loadNotice="THIS SONG USES "+savedName.toUpperCase()+(voiceUnlocked.load()?",|WHICH ISN'T INSTALLED HERE.":",|WHICH ISN'T UNLOCKED HERE.")
                  +"|~PLAYING "+standIn+" INSTEAD.";
        return;
    }
    setVoiceData(std::move(data));
    loadedSelected.store(true);
}
