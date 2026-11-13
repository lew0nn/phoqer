// The dialogs: about, shortcuts, the controls guide, licences, save and open preset, REC and saving a take.
#include "EditorShared.h"

namespace phoqer::ui
{
// "~SAVED IN DOCUMENTS\PHOQER\PRESETS": the folder as PHOQER really uses it, from Documents down.
static juce::String savedIn(const juce::File& folder)
{
    const auto documents = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    const auto shown = folder.isAChildOf(documents) ? "DOCUMENTS" + juce::String(juce::File::getSeparatorString()) + folder.getRelativePathFrom(documents)
                                                    : folder.getFullPathName();
    return "~SAVED IN " + shown.toUpperCase();
}

void PhoqerView::showDialog(Dialog98::Spec spec)
{
    releaseQwertyNotes();
    spec.character = character;
    auto userClose = std::move(spec.onClose);
    spec.onClose = [this, userClose](int button, const juce::String& field)
    {
        dialog = nullptr;                                     // gone before the action, which may open another box
        if (userClose) userClose(button, field);
    };
    dialog = std::make_unique<Dialog98>(std::move(spec));
    addAndMakeVisible(*dialog);
}

void PhoqerView::showMessage(const juce::String& title, const juce::StringArray& lines)
{
    Dialog98::Spec spec;
    spec.title = title;
    spec.lines = lines;
    showDialog(std::move(spec));
}

void PhoqerView::showAbout()
{
    Dialog98::Spec spec;
    spec.title = "ABOUT PHOQER";
    spec.icon = loadAssetImage("phoqer-app-icon-32.png");
    spec.lines = { "PHOQER", "VERSION " JucePlugin_VersionString, "", "~A SYNTHETIC SEAL VOICE", "~LWNX DSP" };
    showDialog(std::move(spec));
}

void PhoqerView::showShortcuts()
{
    Dialog98::Spec spec;
    spec.title = "KEYBOARD SHORTCUTS";
    juce::StringArray play;
    if (isStandalone())     // the computer-key piano is the standalone's
        play = { "#PLAY", "KEYS.EXE|YOUR KEYBOARD IS A PIANO: PRESS THE LETTER ON A KEY",
                 "[Z] [X]|ONE OCTAVE DOWN, ONE UP", "[C] [V]|SOFTER, HARDER" };
    spec.width = 560.0f;
    spec.labelWidth = 190.0f;
    spec.lines = { "#EDIT",
                   "[CTRL]+[Z]|UNDO",
                   "[CTRL]+[Y]|REDO",
                   "[CTRL]+[S]|SAVE A PRESET",
                   "[CTRL]+[O]|OPEN A PRESET",
                   "[ALT]+[E] [V] [P] [H]|OPEN A MENU" };
    play.addArray(spec.lines);
    spec.lines = play;
    showDialog(std::move(spec));
}

void PhoqerView::showGuide()
{
    Dialog98::Spec spec;
    spec.title = "CONTROLS GUIDE";
    spec.width = 560.0f;
    spec.labelWidth = 110.0f;
    spec.lines = { "#KNOBS",
                   "BOOM|THE SEAL'S SIZE: SMALL, OR BIG WITH MORE CHEST",
                   "AIR|BREATH, FROM NONE AT ALL",
                   "BARK|ATTACK AND GROWL: A SOFT SWELL TO A HARD HIT",
                   "SPACE|REVERB: DRY TO A BIG DARK CAVE",
                   "VOWEL|OO, OH, AH, EH, EE",
                   "DETUNE|WIDTH: ONE VOICE TO A WIDE DOUBLE",
                   "TIDE|EXPRESSION: A STRAIGHT NOTE TO SCOOPS AND VIBRATO",
                   "OUTPUT|THE LEVEL",
                   "#MODES",
                   "CALL|ONE CALL PER NOTE",
                   "HONK|NASAL HONKS WHILE THE KEY IS HELD",
                   "BARK|HARD BARKS WHILE THE KEY IS HELD",
                   "WAIL|A LONG GLIDE UP AND BACK",
                   "MURMUR|A MUTTERED, CLOSED-MOUTH HUM",
                   "#METRONOME (STANDALONE APP)",
                   "SWITCH|ABOVE THE SMALL KNOBS, BESIDE REC: THE CLICK ON / OFF; NEVER IN WAV TAKES",
                   "BPM|CLICK IT TO OPEN METRO.EXE; CLICK THE BPM THERE TO TYPE A TEMPO",
                   "SETTINGS|VOLUME, BEATS PER BAR, ACCENT ON BEAT 1, SOUND",
                   "COUNT-IN|1 OR 2 BARS OF CLICKS BEFORE REC STARTS",
                   "REC|RECORD TO WAV; PRESS AGAIN TO STOP AND SAVE" };
    showDialog(std::move(spec));
}

void PhoqerView::showLicences()
{
    Dialog98::Spec spec;
    spec.title = "LICENCES";
    spec.width = 520.0f;
    spec.labelWidth = 240.0f;
    spec.lines = { "#PHOQER IS FREE SOFTWARE",
                   "PHOQER|GNU AGPL 3.0 ONLY",
                   "#BUILT WITH",
                   "JUCE|AGPL 3.0",
                   "VST3 SDK|GPL 3.0",
                   "SILKSCREEN, PRESS START 2P|SIL OFL 1.1",
                   "ROBOTO|APACHE 2.0",
                   "",
                   "~OPEN TEXTS SHOWS THE FULL LICENCES" };
    spec.buttons = { "OPEN TEXTS", "OK" };
    spec.onClose = [](int button, const juce::String&)
    {
        if (button != 0) return;
        // The texts are built into the plugin, so they open even where no files sit beside it.
        const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("PHOQER-Licenses");
        folder.createDirectory();
        for (const auto* file : { "LICENSE", "NOTICE.md", "THIRD_PARTY_NOTICES.md", "OFL-Silkscreen.txt",
                                  "OFL-PressStart2P.txt", "Apache-2.0.txt", "Roboto-NOTICE.txt" })
            for (int i = 0; i < PhoqerUIAssets::namedResourceListSize; ++i)
            {
                const auto* name = PhoqerUIAssets::namedResourceList[i];
                if (juce::String(PhoqerUIAssets::getNamedResourceOriginalFilename(name)) != file) continue;
                int size = 0;
                const auto* data = PhoqerUIAssets::getNamedResource(name, size);
                folder.getChildFile(file).replaceWithData(data, static_cast<size_t>(size));
            }
        folder.startAsProcess();
    };
    showDialog(std::move(spec));
}

void PhoqerView::showSavePreset()
{
    if (dialog != nullptr) return;
    const int current = presets.currentIndex();
    Dialog98::Spec spec;
    spec.title = "SAVE PRESET";
    spec.lines = { "NAME THIS SOUND:", savedIn(PresetLibrary::folder()) };
    spec.fieldLabel = "NAME:";
    spec.fieldText = current >= 0 && ! presets.isBuiltIn(current) ? presets.all()[static_cast<size_t>(current)].name
                                                                 : juce::String("MY ") + voiceNames[character];
    spec.buttons = { "SAVE", "CANCEL" };
    spec.onClose = [this](int button, const juce::String& text) { if (button == 0) savePresetAs(text); };
    showDialog(std::move(spec));
}

void PhoqerView::savePresetAs(const juce::String& rawName)
{
    const auto name = PresetLibrary::cleanName(rawName);
    if (name.isEmpty()) { showMessage("SAVE PRESET", { "TYPE A NAME FIRST.", "~LETTERS, DIGITS, SPACE, - AND _" }); return; }
    const int existing = presets.find(name);
    if (presets.isBuiltIn(existing)) { showMessage("SAVE PRESET", { name + " IS A BUILT-IN PRESET.", "~PICK ANOTHER NAME." }); return; }
    auto write = [this, name]
    {
        if (! presets.save(name)) showMessage("SAVE PRESET", { "COULD NOT SAVE THE PRESET.", "~" + PresetLibrary::folder().getFullPathName().toUpperCase() });
    };
    if (existing < 0) { write(); return; }
    Dialog98::Spec spec;
    spec.title = "SAVE PRESET";
    spec.lines = { name + " ALREADY EXISTS.", "REPLACE IT?" };
    spec.buttons = { "YES", "NO" };
    spec.onClose = [write](int button, const juce::String&) { if (button == 0) write(); };
    showDialog(std::move(spec));
}

void PhoqerView::showOpenPreset()
{
    if (dialog != nullptr || chooser != nullptr) return;
    releaseQwertyNotes();
    PresetLibrary::folder().createDirectory();
    chooser = std::make_unique<juce::FileChooser>("OPEN PRESET", PresetLibrary::folder(), "*.phqpreset");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [safe = juce::Component::SafePointer<PhoqerView>(this)](const juce::FileChooser& fc)
                         {
                             if (safe == nullptr) return;
                             const auto file = fc.getResult();
                             safe->chooser = nullptr;
                             if (file == juce::File()) return;
                             bool ok = false;
                             safe->history.performAsOne([&] { ok = safe->presets.loadFile(file); });
                             if (! ok) safe->showMessage("OPEN PRESET", { "NOT A PHOQER PRESET:", "~" + file.getFileName().toUpperCase() });
                         });
}

void PhoqerView::toggleRecording()
{
    auto& recorder = processor.getRecorder();
    if (! recorder.isRecording())
    {
        if (dialog != nullptr) return;
        recordChannels = juce::jlimit(1, 2, processor.getTotalNumOutputChannels());
        // COUNT-IN: whole bars of clicks first; the take starts on the downbeat after them.
        const double rate = processor.getSampleRate(), bpm = processor.getParameters().getRawParameterValue("standaloneTempo")->load();
        const auto countIn = static_cast<juce::int64>(std::llround(processor.getCountInBars() * processor.getMetronomeBeatsPerBar()
                                                                   * 60.0 / juce::jmax(20.0, bpm) * rate));
        if (! recorder.start(rate, recordChannels, countIn))
            showMessage("RECORD", { "COULD NOT START RECORDING.", "~CHECK THE AUDIO SETUP." });
        return;
    }
    const double length = recorder.seconds(), rate = recorder.sampleRate();
    pendingTake = recorder.stop();
    Dialog98::Spec spec;
    spec.title = "SAVE RECORDING";
    spec.lines = { "SAVE THIS TAKE?",
                   "~LENGTH " + minutesSeconds(length) + "  " + juce::String(juce::roundToInt(rate / 1000.0)) + " KHZ  24-BIT "
                       + (recordChannels == 2 ? "STEREO" : "MONO"),
                   savedIn(Recorder::folder()) };
    spec.fieldLabel = "FILE:";
    spec.fieldText = Recorder::nextFileName(voiceNames[character]).getFileName();
    spec.buttons = { "SAVE", "DISCARD" };
    spec.closable = false;                                    // a take is only lost on purpose
    spec.onClose = [this](int button, const juce::String& text)
    {
        if (button == 0) saveTake(text);
        else { pendingTake.deleteFile(); pendingTake = juce::File(); }
    };
    showDialog(std::move(spec));
}

void PhoqerView::saveTake(const juce::String& rawName)
{
    auto name = juce::File::createLegalFileName(rawName.trim());
    if (name.isEmpty()) name = Recorder::nextFileName(voiceNames[character]).getFileName();
    if (! name.endsWithIgnoreCase(".wav")) name << ".WAV";
    auto folder = Recorder::folder();
    folder.createDirectory();
    auto target = folder.getChildFile(name);
    if (target.exists()) target = target.getNonexistentSibling(false);
    if (pendingTake.moveFileTo(target)) { pendingTake = juce::File(); return; }
    showMessage("SAVE RECORDING", { "COULD NOT SAVE THE TAKE.", "~IT WILL BE SAVED ON EXIT INSTEAD." });
}
}
