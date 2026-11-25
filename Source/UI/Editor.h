#pragma once

#include <JuceHeader.h>

#include "../PluginProcessor.h"
#include "History.h"
#include "Knob.h"
#include "Nitro98.h"
#include "Portrait.h"
#include "Presets.h"
#include "Style.h"
#include "Views.h"
#include "VoiceControls.h"
#include "VoiceLibrary.h"
#include "../Updates.h"

#include <array>
#include <functional>
#include <memory>
#include <vector>

namespace phoqer::ui
{
// What the PHOQER.EXE title bar can do to its window. The standalone app (StandaloneApp.cpp)
// supplies these for its frameless window; in a DAW none are set and the title bar has no buttons.
struct WindowControls
{
    std::function<void()> minimise, toggleFullscreen, close, audioSettings;
    std::function<void(const juce::MouseEvent&)> startDrag, drag;
};

// The editor itself, laid out at a fixed base size (760 wide; shorter when KEYS.EXE is hidden).
// PhoqerEditor scales it to the chosen zoom.
class PhoqerView final : public juce::Component, public juce::FileDragAndDropTarget, public juce::TooltipClient, private juce::Timer
{
public:
    static constexpr int width = 760, fullHeight = 745, compactHeight = 633;     // MIXER.EXE is 15 taller for the KYRAX box

    PhoqerView(PhoqerAudioProcessor&, Win98LookAndFeel&);
    ~PhoqerView() override;

    int baseHeight() const noexcept { return showKeys ? fullHeight : compactHeight; }

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool isKeyDown) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

    void setWindowControls(WindowControls);

    // One UI frame: reads telemetry, animates the face, refreshes the views. Driven by the timer,
    // and called directly by the offline acceptance renderer.
    void tick(double nowSeconds);
    void forceManicForTest(double nowSeconds) { manicUntil = nowSeconds + 0.5; }
    void feedFish();
    void showVoiceBrowser();
    void revealFishForTest() { fishOut = true; logoSlide = fishSlide(); fish.setVisible(true); repaint(); }

    // VIEW menu actions, carried out by the editor that scales this view.
    std::function<float()> currentZoom;
    std::function<void(float)> onZoom;
    std::function<void()> onFitToScreen, onLayoutChanged;

    // UPDATE.EXE (EditorUpdates.cpp). Public so the checks can show each state for a snapshot.
    enum class UpdateStage { none, prompt, downloading, ready, failed, mismatch, upToDate, offline, kyrax };
    void showUpdate(UpdateStage, const updates::Found& = {}, float progressForChecks = -1.0f);
    juce::String updateNotice() const;              // the SCOPE.EXE status field's text when there's an update

private:
    void timerCallback() override;
    void setupUpdates();
    void checkForUpdates(bool manual);
    void updateFound(const updates::Found&, bool manual);
    void layoutUpdate();
    void startUpdateDownload();
    void updatePressed(bool right);
    void updateTick();
    void paintUpdate(juce::Graphics&);
    void openUpdateFromNotice();
    juce::Rectangle<float> noticeArea() const;
    juce::Rectangle<float> keysBar() const;     // KEYS.EXE's status bar
    juce::Rectangle<float> monoArea() const;    // its PLAY box: click to switch MONO / POLY
    juce::String getTooltip() override;
    void applyCharacter(int);
    void refreshVoiceControls();
    void showEnterKey();
    void showRemoveKey();
    void keyChecked(const juce::String& error, int used);   // the activation server's answer, back on the message thread
    void chooseVoice(int);
    void rebuildChrome(float scale);
    void setChoice(const char* id, int index);
    void releaseQwertyNotes();
    bool onTitleBar(juce::Point<float>) const;
    juce::Rectangle<float> logoBounds() const;     // the wordmark at rest
    float fishSlide() const;                       // how far the wordmark moves aside for the fish
    bool isStandalone() const noexcept { return windowControls.audioSettings != nullptr; }
    void setMetroOpen(bool);
    juce::Rectangle<float> nitroBoxArea() const;     // the KYRAX group box (YAP, ARP, NITRO) under the small knobs
    void layoutNitro();
    void openNitro();
    void layoutArp();
    void openArp();
    void refreshArp();     // RATE and HOLD buttons and the step preview follow their settings
    juce::Rectangle<float> transportArea() const;     // [switch | BPM]  [REC | 00:00] above the small knobs (standalone)

    // Menus and the actions behind them.
    void showMenu(int index);
    void showPopup(juce::PopupMenu&, juce::Rectangle<float> target, int menuIndex);
    void showWailBend();
    juce::PopupMenu buildMenu(int index);
    void addPresetItems(juce::PopupMenu&);
    void resetKnobs();
    void randomizeKnobs();
    void stepPreset(int delta);
    void loadPreset(int index);
    void toggleKeys();
    void showAbout();
    void showShortcuts();
    void showGuide();
    void showLicences();
    void panic();
    bool monoOn() const;
    void toggleMono();
    void copySettings();
    void pasteSettings();
    void showSavePreset();
    void savePresetAs(const juce::String& name);
    void showOpenPreset();
    void toggleRecording();
    void saveTake(const juce::String& fileName);
    void showDialog(Dialog98::Spec);
    void showMessage(const juce::String& title, const juce::StringArray& lines);
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int, int) override;
    void chooseVoiceFile();
    void installVoice(const juce::File&);

    PhoqerAudioProcessor& processor;
    Win98LookAndFeel& lookAndFeel;
    PortraitRenderer portrait;
    SealView seal;
    ScopeView scope;
    MeterView meter;
    MenuBar98 menuBar;
    PresetBox98 presetBox;
    RecordControl98 recordControl;
    // The metronome (standalone): beside REC in the header, a switch and the BPM;
    // the BPM opens METRO.EXE, a floating window with the tempo and the settings.
    MetronomeButton98 barMetronome;
    Readout98 barTempo;
    NumberField98 bpmField;
    FloatingWindow98 metroWindow { "METRO.EXE" };
    WailBendPanel98 wailBendPanel;
    // YAP + NITRO (KYRAX only): a group box under the small knobs. Right-click NITRO for NITRO.EXE: the
    // notes on a keyboard, a level for each, WIDTH, STRUM, DETUNE and a STACK quick pick.
    IconToggle98 yapButton { IconToggle98::Icon::lips }, nitroButton { IconToggle98::Icon::bolt };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> yapAttachment, nitroAttachment;
    FloatingWindow98 nitroWindow { "NITRO.EXE" };
    std::unique_ptr<NitroKeys98> nitroKeys;
    std::array<LevelSlider98, 3> nitroLevels;
    // The knobs before their attachments: members are destroyed in reverse order, and an attachment
    // detaches from its knob on the way out (the other order crashed on quit on macOS).
    std::vector<std::unique_ptr<PixelKnob>> nitroKnobs;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> nitroAttachments;
    std::vector<std::unique_ptr<juce::ParameterAttachment>> nitroWatch;
    juce::ComboBox stackBox;
    ActionButton98 nitroOk { "OK" };
    // ARP (KYRAX only), between YAP and NITRO. Right-click ARP for ARP.EXE: a step preview, RATE, HOLD, GATE,
    // SWING, OCTAVES and PATTERN.
    IconToggle98 arpButton { IconToggle98::Icon::notes };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> arpAttachment;
    FloatingWindow98 arpWindow { "ARP.EXE" };
    ArpLanes98 arpSteps;
    std::array<std::unique_ptr<SegmentButton98>, 2> arpLaneModes;     // VEL, GATE: what the step lanes show and edit
    int shownArpLane = -2;
    juce::String shownArpLanes;
    std::array<std::unique_ptr<SegmentButton98>, 5> arpRates;
    std::array<std::unique_ptr<SegmentButton98>, 2> arpHolds;     // OFF, ON
    std::vector<std::unique_ptr<PixelKnob>> arpKnobs;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> arpAttachments;
    std::vector<std::unique_ptr<juce::ParameterAttachment>> arpWatch;
    juce::ComboBox arpPatternBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> arpPatternAttachment;
    ActionButton98 arpOk { "OK" };
    FloatingWindow98 updateWindow { "UPDATE.EXE" };
    ActionButton98 updateLeft { "LATER" }, updateRight { "UPDATE" };
    UpdateStage updateStage = UpdateStage::none;
    updates::Found updateInfo;
    bool checkingUpdates = false;
    float updateTestProgress = -1.0f;
    double downloadStartedMs = 0.0;     // the download screen stays up at least 2 s, so the animation is seen
    juce::File updateFile;
    // The installer download: JUCE's own thread reports here; tick() picks the result up on the message thread.
    struct DownloadWatch final : juce::URL::DownloadTaskListener
    {
        std::atomic<juce::int64> bytes { 0 };
        std::atomic<int> result { 0 };     // 0 running, 1 checked, 2 stopped, 3 didn't match
        updates::Release release;
        juce::File file;
        void progress(juce::URL::DownloadTask*, juce::int64 done, juce::int64) override { bytes = done; }
        void finished(juce::URL::DownloadTask*, bool ok) override { result = ! ok ? 2 : updates::verify(file, release) ? 1 : 3; }
    } downloadWatch;
    std::unique_ptr<juce::URL::DownloadTask> updateDownload;
    DiceButton98 diceButton;     // beside the preset name: EDIT > RANDOMIZE KNOBS in one click
    MetronomeButton98 metronomeButton;
    bool metronomeWasOn = false;
    juce::Slider metronomeVolume { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::ComboBox beatsBox, soundBox, countInBox;
    juce::ToggleButton accentBox { "ACCENT BEAT 1" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> metronomeAttachment, barMetronomeAttachment, accentAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volumeAttachment;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> metroBoxAttachments;
    int shownBeatLight = -1;
    Fish98 fish;
    BlueScreen98 blueScreen;
    PianoView piano;
    std::array<std::unique_ptr<VoiceSlot98>, 4> voiceTabs;
    ActionButton98 browseVoices { "=" }, tryOnlineButton { "TRY ONLINE" }, loadVoiceButton { "LOAD VOICE" }, useVoiceButton { "USE" }, keyButton { "GET ON ITCH" };
    FloatingWindow98 voiceBrowser { "VOICES.EXE" };
    std::array<std::unique_ptr<VoiceLibraryRow98>,4> voiceRows;
    juce::ToggleButton quickVoice { "IN QUICK BAR" };
    juce::String voiceControlsKey;     // what the voice strip last showed (refreshVoiceControls)
    int browserVoice=3;
    std::array<std::unique_ptr<ChoiceButton>, 5> modeButtons;
    std::array<std::unique_ptr<TitleButton98>, 3> titleButtons;    // minimise, maximise, close
    WindowControls windowControls;
    bool draggingWindow = false;
    std::vector<std::unique_ptr<PixelKnob>> knobs;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    juce::ParameterAttachment characterAttachment;
    juce::TooltipWindow tooltips { this, 700 };

    PresetLibrary presets;
    ParamHistory history;
    std::unique_ptr<Dialog98> dialog;
    bool checkingKey = false;                                // the CHECKING box is up
    std::unique_ptr<juce::FileChooser> chooser;
    juce::File pendingTake;                                         // a stopped recording awaiting SAVE / DISCARD
    int recordChannels = 2;
    juce::String shownPreset;

    int character = 0;
    bool showKeys = true;
    juce::Image chrome, logoWord;
    float chromeScale = 0.0f;
    FaceTelemetry face {};
    float grin = 0.0f;
    float shownMouth = 0.0f;      // the mouth as the portrait last drew it
    bool fishOut = false;
    double fishBackAt = 0.0;     // eaten: the next fish comes out from behind the wordmark then
    std::array<double, 5> lastFed {};     // when the last five fish were eaten; five in 30 s is too many
    double crashAt = 0.0;                 // overfed: the blue screen comes up then
    double happyFrom = 0.0, happyUntil = 0.0;     // fed: the seal sways and squints happily
    void rebootSeal();
    float logoSlide = 0.0f;
    double manicUntil = 0.0, watchUntil = 0.0;
    uint32_t lastCallSerial = 0;
    int frame = 0;

    // Computer-keyboard playing: base note of the A key, velocity, and the note each key is holding.
    int qwertyBase = 60, qwertyVelocity = 120;
    std::array<int, qwertyKeyCount> heldNotes;
    std::array<bool, 5> controlKeysDown {};    // Z X C V M
    bool shownMono = false;                      // what the KEYS.EXE status bar last showed

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhoqerView)
};

// PHOQER's "Pure 98" editor: the view above, scaled
// to the saved zoom, with a Win98 size grip. The zoom is kept in UiSettings for the next launch.
class PhoqerEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PhoqerEditor(PhoqerAudioProcessor&);
    ~PhoqerEditor() override;

    void resized() override;

    void setWindowControls(WindowControls c) { view.setWindowControls(std::move(c)); }
    void tick(double nowSeconds) { view.tick(nowSeconds); }
    void forceManicForTest(double nowSeconds) { view.forceManicForTest(nowSeconds); }
    PhoqerView& getView() noexcept { return view; }

    static constexpr float minZoom() noexcept { return 0.75f; }
    static constexpr float maxZoom() noexcept { return 4.0f; }
    void setZoom(float);
    float getZoom() const noexcept { return zoom; }
    void fitToScreen();

private:
    void updateLimits();
    juce::Rectangle<int> screenArea() const;

    Win98LookAndFeel laf;
    PhoqerView view;
    float zoom = 1.0f;
    bool remembersZoom = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhoqerEditor)
};
}
