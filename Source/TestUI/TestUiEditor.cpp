#include "TestUiEditor.h"

#include "TestUiLogo.h"
#include "TestUiRecorder.h"
#include "TestUiSettings.h"

namespace phoqer::testui
{
namespace
{
// Window frames (view coordinates; KEYS.EXE is left out when hidden).
const juce::Rectangle<float> headWindow { 6, 4, 748, 90 };
const juce::Rectangle<float> modeWindow { 6, 100, 86, 278 };
const juce::Rectangle<float> sealWindow { 98, 100, 262, 278 };
const juce::Rectangle<float> scopeWindow { 366, 100, 388, 194 };
const juce::Rectangle<float> outputWindow { 366, 300, 388, 78 };
const juce::Rectangle<float> mixerWindow { 6, 384, 748, 228 };
const juce::Rectangle<float> keysWindow { 6, 618, 748, 106 };

// The main window's title bar is taller than the others' so its real caption buttons are easy to hit.
constexpr float headTitleHeight = 22.0f;
constexpr float captionButtonW = 20.0f, captionButtonH = 18.0f;


// Header: the PHOQER wordmark at 2 units per pixel like the rest of the UI, a little in from the left.
constexpr float logoWordPixel = 2.0f, logoWordInset = 16.0f;

// MANIC: the first 150 ms of a new call in BARK mode above this face intensity flashes the grin.
constexpr float manicIntensity = 0.85f;
constexpr double manicWatchSeconds = 0.15, manicHoldSeconds = 0.5;
constexpr int barkMode = 2;

const char* voiceNames[] { "BURP", "SQUEAL", "GROAN" };

juce::RangedAudioParameter& parameterFor(juce::AudioProcessorValueTreeState& state, const char* id)
{
    auto* p = state.getParameter(id);
    jassert(p != nullptr);
    return *p;
}

juce::String percent(double v) { return juce::String(juce::roundToInt(v * 100.0)) + "%"; }
juce::String decibels(double v) { return (v > 0.0 ? "+" : "") + juce::String(v, 1) + " dB"; }
juce::String noteName(int note) { return juce::MidiMessage::getMidiNoteName(note, true, true, 4); }

constexpr char controlKeys[] { 'z', 'x', 'c', 'v' };    // octave down/up, velocity down/up
constexpr int lowestBase = 24, highestBase = 96, velocityStep = 10;

// Header row: the preset box sits between the logo and the voice tabs; REC at the menu row's end.
constexpr float presetBoxX = 224.0f, presetBoxW = 224.0f;
constexpr float recordW = 112.0f;

// Menu item IDs. Radio items (one of several) live in Win98LookAndFeel's radio range.
constexpr float zoomSteps[] { 1.0f, 1.25f, 1.5f, 2.0f };
constexpr int zoomIdFirst = Win98LookAndFeel::radioIdFirst + 1;
constexpr int presetIdFirst = Win98LookAndFeel::radioIdFirst + 100;

// The sound knobs RANDOMIZE touches (never OUTPUT, so it cannot jump your level).
const char* soundKnobs[] { "boom", "air", "bark", "space", "vowel", "detune", "tide" };

bool modifierHeld()
{
    const auto mods = juce::ModifierKeys::getCurrentModifiersRealtime();
    return mods.isCtrlDown() || mods.isAltDown() || mods.isCommandDown();
}

juce::String minutesSeconds(double seconds)
{
    const int s = static_cast<int>(seconds);
    return juce::String(s / 60).paddedLeft('0', 2) + ":" + juce::String(s % 60).paddedLeft('0', 2);
}
}

// ===================================================================================== VIEW
TestUiView::TestUiView(PhoqerAudioProcessor& owner, Win98LookAndFeel& laf)
    : processor(owner), lookAndFeel(laf), scope(owner.getTelemetry()), meter(owner.getTelemetry()),
      piano(owner.getKeyboardState()),
      characterAttachment(parameterFor(owner.getParameters(), "character"),
                          [this](float value) { applyCharacter(juce::roundToInt(value)); }, nullptr),
      presets(owner.getParameters()),
      history(owner, [this] { return presets.loadedName(); }, [this](const juce::String& name) { presets.select(name); })
{
    setOpaque(true);
    auto& state = processor.getParameters();
    lastCallSerial = processor.getTelemetry().getCallSerial();
    showKeys = UiSettings::get().showKeys();

    const int voiceOrder[] { 0, 1, 2 };    // BURP first: it is the default voice
    for (size_t slot = 0; slot < voiceTabs.size(); ++slot)
    {
        const int c = voiceOrder[slot];
        voiceTabs[slot] = std::make_unique<ChoiceButton>(ChoiceButton::Kind::voiceTab, c, voiceNames[c], parameterFor(state, "character"));
        addAndMakeVisible(*voiceTabs[slot]);
    }
    const char* modes[] { "CALL", "HONK", "BARK", "WAIL", "MURMUR" };
    for (size_t m = 0; m < modeButtons.size(); ++m)
    {
        modeButtons[m] = std::make_unique<ChoiceButton>(ChoiceButton::Kind::mode, static_cast<int>(m), modes[m], parameterFor(state, "behavior"));
        addAndMakeVisible(*modeButtons[m]);
    }

    struct KnobDef { const char* id; const char* name; const char* description; bool big; };
    const KnobDef defs[] { { "boom", "BOOM", "LOW BODY", true }, { "air", "AIR", "BREATH", true }, { "bark", "BARK", "GROWL", true },
                           { "space", "SPACE", "ROOM", true }, { "vowel", "VOWEL", "FORMANT", false }, { "detune", "DETUNE", "SPREAD", false },
                           { "tide", "TIDE", "MOTION", false }, { "output", "OUTPUT", "LEVEL", false } };
    for (const auto& def : defs)
    {
        PixelKnob::Spec spec;
        spec.name = def.name;
        spec.description = def.description;
        const bool isOutput = juce::String(def.id) == "output";
        spec.radius = def.big ? 34.0f : isOutput ? 13.0f : 19.0f;
        spec.centreY = 82.0f;
        spec.big = def.big;
        spec.layout = isOutput ? PixelKnob::Layout::output : PixelKnob::Layout::mixer;
        spec.format = isOutput ? decibels : percent;
        auto knob = std::make_unique<PixelKnob>(spec);
        addAndMakeVisible(*knob);
        sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, def.id, *knob));
        auto& p = parameterFor(state, def.id);
        knob->setDoubleClickReturnValue(true, p.convertFrom0to1(p.getDefaultValue()));
        knobs.push_back(std::move(knob));
    }

    for (auto* view : std::initializer_list<juce::Component*> { &seal, &scope, &meter, &menuBar, &presetBox })
        addAndMakeVisible(*view);
    addChildComponent(piano);
    piano.setVisible(showKeys);
    addChildComponent(recordControl);                                   // shown in the standalone app only
    const TitleButton98::Kind kinds[] { TitleButton98::Kind::minimise, TitleButton98::Kind::maximise, TitleButton98::Kind::close };
    for (size_t k = 0; k < titleButtons.size(); ++k)
        titleButtons[k] = std::make_unique<TitleButton98>(kinds[k]);    // shown once the window supplies controls
    titleButtons[0]->onClick = [this] { if (windowControls.minimise) windowControls.minimise(); };
    titleButtons[1]->onClick = [this] { if (windowControls.toggleFullscreen) windowControls.toggleFullscreen(); };
    titleButtons[2]->onClick = [this] { if (windowControls.close) windowControls.close(); };
    for (auto& b : titleButtons) addChildComponent(*b);
    heldNotes.fill(-1);

    menuBar.onOpen = [this](int i) { showMenu(i); };
    presetBox.text = [this] { return presets.displayName(); };
    presetBox.onPrevious = [this] { stepPreset(-1); };
    presetBox.onNext = [this] { stepPreset(1); };
    presetBox.onOpenList = [this]
    {
        juce::PopupMenu m;
        addPresetItems(m);
        showPopup(m, presetBox.getBounds().toFloat().withTrimmedRight(38.0f), -1);
    };
    recordControl.onToggle = [this] { toggleRecording(); };

    // Key presses always land on the view so the computer keyboard keeps playing after clicks.
    for (auto* child : getChildren())
    {
        child->setWantsKeyboardFocus(false);
        child->setMouseClickGrabsKeyboardFocus(false);
    }
    setWantsKeyboardFocus(true);

    characterAttachment.sendInitialUpdate();
    setSize(width, baseHeight());
    tick(juce::Time::getMillisecondCounterHiRes() * 0.001);
    startTimerHz(30);
}

TestUiView::~TestUiView()
{
    stopTimer();
    releaseQwertyNotes();
    // Never lose a take: one still running, or waiting on SAVE / DISCARD, is saved under the next free name.
    auto& recorder = processor.getRecorder();
    if (recorder.isRecording()) pendingTake = recorder.stop();
    if (pendingTake.existsAsFile())
    {
        Recorder::folder().createDirectory();
        pendingTake.moveFileTo(Recorder::nextFileName(voiceNames[character]));
    }
    dialog = nullptr;
}

void TestUiView::releaseQwertyNotes()
{
    for (auto& note : heldNotes)
        if (note >= 0) { processor.getKeyboardState().noteOff(1, note, 0.0f); note = -1; }
}

bool TestUiView::keyStateChanged(bool)
{
    // No notes while a box is open (its text field gets the typing) or while Ctrl / Alt is held for a shortcut.
    if (dialog != nullptr) { releaseQwertyNotes(); return false; }
    const bool shortcut = modifierHeld();
    bool used = false;
    for (int i = 0; i < qwertyKeyCount; ++i)
    {
        auto& held = heldNotes[static_cast<size_t>(i)];
        const bool down = juce::KeyPress::isKeyCurrentlyDown(qwertyKeys[i]);
        if (down && held < 0 && ! shortcut)
        {
            held = qwertyBase + i;
            processor.getKeyboardState().noteOn(1, held, static_cast<float>(qwertyVelocity) / 127.0f);
            used = true;
        }
        else if (! down && held >= 0)
        {
            processor.getKeyboardState().noteOff(1, held, 0.0f);
            held = -1;
            used = true;
        }
    }
    // Octave and velocity change once per press, not on key repeat. Held notes keep their pitch.
    for (size_t k = 0; k < controlKeysDown.size(); ++k)
    {
        const bool down = juce::KeyPress::isKeyCurrentlyDown(controlKeys[k]);
        if (down && ! controlKeysDown[k] && ! shortcut)
        {
            if (k < 2) qwertyBase = juce::jlimit(lowestBase, highestBase, qwertyBase + (k == 0 ? -12 : 12));
            else qwertyVelocity = juce::jlimit(velocityStep, 127, qwertyVelocity + (k == 2 ? -velocityStep : velocityStep));
            piano.setQwertyBase(qwertyBase);
            repaint(windowClient(keysWindow).toNearestInt());
            used = true;
        }
        controlKeysDown[k] = down;
    }
    return used;
}

void TestUiView::setWindowControls(WindowControls controls)
{
    windowControls = std::move(controls);
    titleButtons[0]->setVisible(windowControls.minimise != nullptr);
    titleButtons[1]->setVisible(windowControls.toggleFullscreen != nullptr);
    titleButtons[2]->setVisible(windowControls.close != nullptr);
    recordControl.setVisible(isStandalone());
}

bool TestUiView::onTitleBar(juce::Point<float> p) const
{
    return windowTitleBar(headWindow, headTitleHeight).contains(p);
}

// The PHOQER.EXE title bar moves the standalone window, and double-clicking it toggles fullscreen.
void TestUiView::mouseDown(const juce::MouseEvent& e)
{
    draggingWindow = windowControls.startDrag != nullptr && onTitleBar(e.position);
    if (draggingWindow) windowControls.startDrag(e);
}

void TestUiView::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingWindow && windowControls.drag) windowControls.drag(e);
}

void TestUiView::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (windowControls.toggleFullscreen && onTitleBar(e.position)) windowControls.toggleFullscreen();
}

bool TestUiView::keyPressed(const juce::KeyPress& key)
{
    if (dialog != nullptr) return true;
    using KeyPress = juce::KeyPress;
    constexpr int cmd = juce::ModifierKeys::commandModifier, shift = juce::ModifierKeys::shiftModifier;
    if (key == KeyPress('z', cmd, 0)) { history.undo(); return true; }
    if (key == KeyPress('y', cmd, 0) || key == KeyPress('z', cmd | shift, 0)) { history.redo(); return true; }
    if (key == KeyPress('s', cmd, 0)) { showSavePreset(); return true; }
    if (key == KeyPress('o', cmd, 0)) { showOpenPreset(); return true; }

    const auto c = static_cast<char>(juce::CharacterFunctions::toLowerCase(static_cast<juce::juce_wchar>(key.getKeyCode())));
    // Alt + the underlined letter opens that menu, as in Windows.
    if (key.getModifiers().isAltDown() && ! key.getModifiers().isCtrlDown())
        for (int i = 0; i < MenuBar98::itemCount; ++i)
            if (c == static_cast<char>(juce::CharacterFunctions::toLowerCase(static_cast<juce::juce_wchar>(MenuBar98::items[i][0]))))
            {
                showMenu(i);
                return true;
            }
    // Swallow the playing keys so the host or OS does not also act on them.
    return ! key.getModifiers().isCommandDown() && ! key.getModifiers().isAltDown() && c != 0
        && (juce::String(qwertyKeys).containsChar(c) || juce::String("zxcv").containsChar(c));
}

void TestUiView::setChoice(const char* id, int index)
{
    auto& p = parameterFor(processor.getParameters(), id);
    p.beginChangeGesture();
    p.setValueNotifyingHost(p.convertTo0to1(static_cast<float>(index)));
    p.endChangeGesture();
}

void TestUiView::applyCharacter(int c)
{
    character = juce::jlimit(0, 2, c);
    lookAndFeel.setCharacter(character);
    logoWord = renderOutrunWordmark(character, false);
    for (auto& k : knobs) k->setCharacter(character);
    for (auto& b : modeButtons) b->setCharacter(character);
    scope.setCharacter(character);
    meter.setCharacter(character);
    piano.setCharacter(character);
    chrome = {};
    repaint();
}

void TestUiView::resized()
{
    const auto client = [](juce::Rectangle<float> r) { return windowClient(r); };
    const auto head = windowClient(headWindow, headTitleHeight);
    menuBar.setBounds(juce::Rectangle<float>(head.getX(), head.getY() - 2.0f, head.getWidth(), 17.0f).toNearestInt());
    recordControl.setBounds(juce::Rectangle<float>(head.getRight() - 2.0f - recordW, head.getY() - 2.0f, recordW, 15.0f).toNearestInt());
    const auto row = head.withTrimmedTop(17.0f);
    for (size_t slot = 0; slot < voiceTabs.size(); ++slot)
        voiceTabs[slot]->setBounds(juce::Rectangle<float>(row.getRight() - 4.0f - (3 - slot) * 94.0f + 4.0f, row.getCentreY() - 12.0f, 90.0f, 24.0f).toNearestInt());
    presetBox.setBounds(juce::Rectangle<float>(presetBoxX, std::round(row.getCentreY() - 12.0f), presetBoxW, 24.0f).toNearestInt());

    const auto tools = client(modeWindow);
    for (size_t m = 0; m < modeButtons.size(); ++m)
        modeButtons[m]->setBounds(juce::Rectangle<float>(tools.getX() + 2.0f, tools.getY() + 2.0f + m * 49.0f, tools.getWidth() - 4.0f, 46.0f).toNearestInt());

    seal.setBounds(client(sealWindow).withTrimmedBottom(18.0f).reduced(3.0f).toNearestInt());
    scope.setBounds(client(scopeWindow).withTrimmedBottom(18.0f).reduced(2.0f).toNearestInt());
    const auto out = client(outputWindow);
    meter.setBounds(juce::Rectangle<float>(out.getX() + 2.0f, out.getY() + 4.0f, 236.0f, 40.0f).toNearestInt());
    knobs[7]->setBounds(juce::Rectangle<float>(out.getRight() - 136.0f, out.getY(), 136.0f, out.getHeight()).toNearestInt());

    const auto mix = client(mixerWindow);
    for (int i = 0; i < 7; ++i)
    {
        const bool big = i < 4;
        const auto cell = big ? juce::Rectangle<float>(mix.getX() + 4.0f + i * 134.0f, mix.getY(), 134.0f, mix.getHeight())
                              : juce::Rectangle<float>(mix.getX() + 544.0f + (i - 4) * 64.0f, mix.getY(), 64.0f, mix.getHeight());
        knobs[static_cast<size_t>(i)]->setBounds(cell.toNearestInt());
    }
    const auto keys = client(keysWindow);
    const float pianoWidth = std::floor((keys.getWidth() - 6.0f) / PianoView::whiteKeysShown) * PianoView::whiteKeysShown;
    piano.setBounds(juce::Rectangle<float>(pianoWidth, keys.getHeight() - 24.0f).withCentre({ keys.getCentreX(), keys.getY() + 3.0f + (keys.getHeight() - 24.0f) * 0.5f }).toNearestInt());
    const auto bar = windowTitleBar(headWindow, headTitleHeight);
    for (size_t k = 0; k < titleButtons.size(); ++k)
    {
        // Win98 spacing: minimise and maximise touch, close stands 2 px apart.
        const float right = bar.getRight() - 2.0f - static_cast<float>(2 - k) * captionButtonW - (k < 2 ? 2.0f : 0.0f);
        titleButtons[k]->setBounds(juce::Rectangle<float>(right - captionButtonW, bar.getY() + 2.0f, captionButtonW, captionButtonH).toNearestInt());
    }
    if (dialog != nullptr) dialog->setBounds(getLocalBounds());
}

void TestUiView::rebuildChrome(float scale)
{
    const int height = baseHeight();
    chromeScale = scale;
    chrome = juce::Image(juce::Image::RGB, juce::roundToInt(width * scale), juce::roundToInt(height * scale), true);
    juce::Graphics g(chrome);
    g.addTransform(juce::AffineTransform::scale(scale));
    const auto& pal = paletteFor(character);
    const juce::String voice = voiceNames[character];

    // Desktop: night sky, stars, neon perspective grid.
    g.fillAll(pal.sky);
    juce::Random stars(5);
    for (int k = 0; k < 120; ++k)
    {
        g.setColour(juce::Colours::white.withAlpha(0.2f + 0.5f * stars.nextFloat()));
        g.fillRect(std::round(stars.nextFloat() * width / 2.0f) * 2.0f, std::round(stars.nextFloat() * height / 2.0f) * 2.0f, 2.0f, 2.0f);
    }
    for (const auto& [alpha, thickness] : { std::pair<float, float> { 0.12f, 4.0f }, { 0.5f, 1.0f } })
    {
        g.setColour(pal.accent.withAlpha(alpha));
        for (int k = -18; k <= 18; ++k) g.drawLine(width * 0.5f + k * 8.0f, 300.0f, width * 0.5f + k * 95.0f, static_cast<float>(height), thickness);
        for (int k = 0; k < 9; ++k)
        {
            const float y = 300.0f + (height - 300.0f) * std::pow(k / 8.0f, 2.2f);
            g.drawLine(0.0f, y, static_cast<float>(width), y, thickness);
        }
    }

    // Header window: the PHOQER wordmark; the preset box and voice tabs sit on the right.
    const auto head = window98(g, headWindow, "PHOQER.EXE - " + voice, pal, true, headTitleHeight);
    const auto row = head.withTrimmedTop(17.0f);
    g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
    g.setOpacity(1.0f);
    const float wordW = logoWord.getWidth() * logoWordPixel, wordH = logoWord.getHeight() * logoWordPixel;
    g.drawImage(logoWord, { row.getX() + logoWordInset, std::round(row.getCentreY() - wordH * 0.5f), wordW, wordH });

    window98(g, modeWindow, "MODE.EXE", pal, false);
    const auto sealClient = window98(g, sealWindow, voice + ".BMP", pal, false);
    sunken(g, sealClient.withTrimmedBottom(18.0f), juce::Colours::black);
    const auto scopeClient = window98(g, scopeWindow, "SCOPE.EXE", pal, false);
    sunken(g, scopeClient.withTrimmedBottom(18.0f), juce::Colours::black);
    window98(g, outputWindow, "LEVEL.EXE", pal, false);
    const auto mix = window98(g, mixerWindow, "MIXER.EXE", pal, false);
    g.setColour(win98::shadow);
    g.fillRect(mix.getX() + 541.0f, mix.getY() + 14.0f, 1.0f, mix.getHeight() - 28.0f);
    g.setColour(win98::light);
    g.fillRect(mix.getX() + 542.0f, mix.getY() + 14.0f, 1.0f, mix.getHeight() - 28.0f);
    if (showKeys)
    {
        window98(g, keysWindow, "KEYS.EXE", pal, false);
        sunken(g, piano.getBounds().toFloat().expanded(2.0f), win98::dark);
    }
}

void TestUiView::paint(juce::Graphics& g)
{
    // The static chrome is cached at the physical pixel scale it is drawn at (zoom included).
    const float physical = juce::jmax(1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (! chrome.isValid() || ! juce::approximatelyEqual(physical, chromeScale)) rebuildChrome(physical);
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(chrome, getLocalBounds().toFloat());
    // Live status bars.
    const auto sealClient = windowClient(sealWindow), scopeClient = windowClient(scopeWindow);
    statusBar(g, sealClient.withTrimmedTop(sealClient.getHeight() - 15.0f),
              { juce::String(seal.getWidth() / 2) + "x" + juce::String(seal.getHeight() / 2) + " PIXELS",
                "MTH " + juce::String(juce::roundToInt(face.mouthOpen * 100.0f)) + "%" });
    statusBar(g, scopeClient.withTrimmedTop(scopeClient.getHeight() - 15.0f),
              { juce::String(juce::roundToInt(processor.getTelemetry().getSampleRate() / 1000.0f)) + " kHz",
                juce::String(juce::roundToInt(scope.capturedMilliseconds())) + " ms", "TRIG NOTE" });
    if (showKeys)
    {
        const auto keysClient = windowClient(keysWindow);
        statusBar(g, keysClient.withTrimmedTop(keysClient.getHeight() - 15.0f),
                  { "TYPE A TO ; TO PLAY", "Z/X OCTAVE  A=" + noteName(qwertyBase), "C/V VELOCITY " + juce::String(qwertyVelocity) });
    }
}

void TestUiView::timerCallback()
{
    tick(juce::Time::getMillisecondCounterHiRes() * 0.001);
}

void TestUiView::tick(double now)
{
    const auto& telemetry = processor.getTelemetry();
    const auto target = telemetry.readFace();
    auto follow = [](float& value, float goal) { value += (goal - value) * 0.55f; };
    follow(face.mouthOpen, target.mouthOpen);
    follow(face.mouthRound, target.mouthRound);
    follow(face.jawWidth, target.jawWidth);
    follow(face.throatTension, target.throatTension);
    follow(face.eyeSquint, target.eyeSquint);
    follow(face.eyeOpen, target.eyeOpen);
    follow(face.headLift, target.headLift);
    follow(face.intensity, target.intensity);

    // MANIC: watch the start of each new call; a hard BARK hit flashes the grin.
    const auto serial = telemetry.getCallSerial();
    if (serial != lastCallSerial)
    {
        lastCallSerial = serial;
        watchUntil = now + manicWatchSeconds;
    }
    const int mode = juce::roundToInt(parameterFor(processor.getParameters(), "behavior").convertFrom0to1(
        parameterFor(processor.getParameters(), "behavior").getValue()));
    if (now < watchUntil && mode == barkMode && target.intensity >= manicIntensity)
        manicUntil = now + manicHoldSeconds;
    const float previousGrin = grin;
    grin += ((now < manicUntil ? 1.0f : 0.0f) - grin) * 0.6f;
    if (grin < 0.01f) grin = 0.0f;

    // Idle eyes rest half open; telemetry opens them further.
    Expression ex { face, grin };
    ex.face.eyeOpen = 0.45f + 0.55f * face.eyeOpen;
    if ((frame % 2) == 0 || std::abs(grin - previousGrin) > 0.05f)
        seal.update(portrait, character, ex);
    scope.refresh();
    meter.refresh();

    const auto& recorder = processor.getRecorder();
    recordControl.setState(recorder.isRecording(), recorder.isRecording() ? recorder.seconds() : 0.0);
    if (const auto text = presets.displayName(); text != shownPreset)
    {
        shownPreset = text;
        presetBox.repaint();
    }

    // Keep the computer keyboard live: take focus whenever the window is active, and let go of
    // held notes when it is not (key-up events would never arrive).
    if (auto* peer = getPeer(); peer != nullptr && peer->isFocused())
    {
        if (! hasKeyboardFocus(true) && isShowing()) grabKeyboardFocus();
    }
    else
    {
        releaseQwertyNotes();
        controlKeysDown = {};
    }
    repaint(windowClient(sealWindow).withTrimmedTop(windowClient(sealWindow).getHeight() - 15.0f).toNearestInt());
    repaint(windowClient(scopeWindow).withTrimmedTop(windowClient(scopeWindow).getHeight() - 15.0f).toNearestInt());
    ++frame;
}

// ------------------------------------------------------------------------------------------ MENUS
namespace
{
void addItem(juce::PopupMenu& menu, const juce::String& text, int id, bool enabled, bool ticked, const juce::String& shortcut,
             std::function<void()> action)
{
    juce::PopupMenu::Item item(text);
    item.itemID = id;
    item.isEnabled = enabled;
    item.isTicked = ticked;
    item.shortcutKeyDescription = shortcut;
    item.action = std::move(action);
    menu.addItem(std::move(item));
}
}

void TestUiView::addPresetItems(juce::PopupMenu& menu)
{
    presets.refresh();
    const auto& all = presets.all();
    for (size_t i = 0; i < all.size(); ++i)
    {
        const int index = static_cast<int>(i);
        if (index > 0 && ! presets.isBuiltIn(index) && presets.isBuiltIn(index - 1)) menu.addSeparator();    // your own presets
        addItem(menu, PresetLibrary::numbered(index, all[i].name), presetIdFirst + index, true, index == presets.currentIndex(), {},
                [this, index] { loadPreset(index); });
    }
}

juce::PopupMenu TestUiView::buildMenu(int index)
{
    juce::PopupMenu m;
    int id = 1;
    switch (index)
    {
        case 0:    // EDIT
            addItem(m, "UNDO", id++, history.canUndo(), false, "CTRL+Z", [this] { history.undo(); });
            addItem(m, "REDO", id++, history.canRedo(), false, "CTRL+Y", [this] { history.redo(); });
            m.addSeparator();
            addItem(m, "RESET ALL KNOBS", id++, true, false, {}, [this] { resetKnobs(); });
            addItem(m, "RANDOMIZE KNOBS", id++, true, false, {}, [this] { randomizeKnobs(); });
            m.addSeparator();
            addItem(m, "AUDIO/MIDI SETUP...", id++, windowControls.audioSettings != nullptr, false, {},
                    [this] { if (windowControls.audioSettings) windowControls.audioSettings(); });
            break;

        case 1:    // VIEW
        {
            const float zoom = currentZoom ? currentZoom() : 1.0f;
            for (int z = 0; z < static_cast<int>(std::size(zoomSteps)); ++z)
            {
                const float step = zoomSteps[z];
                addItem(m, "ZOOM " + juce::String(juce::roundToInt(step * 100.0f)) + "%", zoomIdFirst + z, true,
                        std::abs(zoom - step) < 0.005f, {}, [this, step] { if (onZoom) onZoom(step); });
            }
            addItem(m, "FIT TO SCREEN", id++, true, false, {}, [this] { if (onFitToScreen) onFitToScreen(); });
            m.addSeparator();
            addItem(m, "START IN FULL SCREEN", id++, windowControls.toggleFullscreen != nullptr, UiSettings::get().startFullscreen(), {},
                    [] { UiSettings::get().setStartFullscreen(! UiSettings::get().startFullscreen()); });
            m.addSeparator();
            addItem(m, "KEYS.EXE", id++, true, showKeys, {}, [this] { toggleKeys(); });
            break;
        }

        case 2:    // PRESET
            addItem(m, "SAVE PRESET...", id++, true, false, "CTRL+S", [this] { showSavePreset(); });
            addItem(m, "OPEN PRESET...", id++, true, false, "CTRL+O", [this] { showOpenPreset(); });
            addItem(m, "INIT", id++, true, false, {}, [this] { loadPreset(0); });
            m.addSeparator();
            addPresetItems(m);
            break;

        case 3:    // HELP
            addItem(m, "ABOUT PHOQER...", id++, true, false, {}, [this] { showAbout(); });
            break;

        default: break;
    }
    return m;
}

void TestUiView::showMenu(int index)
{
    if (dialog != nullptr || ! juce::isPositiveAndBelow(index, MenuBar98::itemCount)) return;
    auto menu = buildMenu(index);
    const auto title = menuBar.itemBounds(index);
    showPopup(menu, title.translated(static_cast<float>(menuBar.getX()), static_cast<float>(menuBar.getY())), index);
}

// Drops a menu down from an area of this view. The target component makes the menu scale with the zoom.
void TestUiView::showPopup(juce::PopupMenu& menu, juce::Rectangle<float> target, int menuIndex)
{
    releaseQwertyNotes();
    menu.setLookAndFeel(&lookAndFeel);
    menuBar.setOpenIndex(menuIndex);
    const auto screenArea = localAreaToGlobal(target.toNearestInt());
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea(screenArea)
                                                 .withMinimumWidth(140),
                       [safe = juce::Component::SafePointer<TestUiView>(this)](int)
                       {
                           if (safe != nullptr) safe->menuBar.setOpenIndex(-1);
                       });
}

// ---------------------------------------------------------------------------------------- ACTIONS
void TestUiView::resetKnobs()
{
    history.performAsOne([this]
    {
        for (const auto* id : { "boom", "air", "bark", "space", "vowel", "detune", "tide", "output" })
        {
            auto& p = parameterFor(processor.getParameters(), id);
            p.beginChangeGesture();
            p.setValueNotifyingHost(p.getDefaultValue());
            p.endChangeGesture();
        }
    });
}

void TestUiView::randomizeKnobs()
{
    history.performAsOne([this]
    {
        auto& random = juce::Random::getSystemRandom();
        for (const auto* id : soundKnobs)
        {
            auto& p = parameterFor(processor.getParameters(), id);
            p.beginChangeGesture();
            p.setValueNotifyingHost(random.nextFloat());
            p.endChangeGesture();
        }
    });
}

void TestUiView::loadPreset(int index)
{
    history.performAsOne([this, index] { presets.load(index); });
}

void TestUiView::stepPreset(int delta)
{
    presets.refresh();
    const int count = static_cast<int>(presets.all().size());
    if (count == 0) return;
    const int from = presets.currentIndex() < 0 ? (delta > 0 ? -1 : 0) : presets.currentIndex();
    loadPreset(((from + delta) % count + count) % count);
}

void TestUiView::toggleKeys()
{
    showKeys = ! showKeys;
    UiSettings::get().setShowKeys(showKeys);
    if (! showKeys) releaseQwertyNotes();
    piano.setVisible(showKeys);
    setSize(width, baseHeight());
    chrome = {};
    repaint();
    if (onLayoutChanged) onLayoutChanged();
}

void TestUiView::showDialog(Dialog98::Spec spec)
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

void TestUiView::showMessage(const juce::String& title, const juce::StringArray& lines)
{
    Dialog98::Spec spec;
    spec.title = title;
    spec.lines = lines;
    showDialog(std::move(spec));
}

void TestUiView::showAbout()
{
    Dialog98::Spec spec;
    spec.title = "ABOUT PHOQER";
    spec.icon = loadAssetImage("phoqer-app-icon-32.png");
    spec.lines = { "PHOQER", "VERSION " JucePlugin_VersionString, "", "~A SYNTHETIC SEAL VOICE", "~LWNX DSP" };
    showDialog(std::move(spec));
}

void TestUiView::showSavePreset()
{
    if (dialog != nullptr) return;
    const int current = presets.currentIndex();
    Dialog98::Spec spec;
    spec.title = "SAVE PRESET";
    spec.lines = { "NAME THIS SOUND:", "~SAVED IN DOCUMENTS\\PHOQER\\PRESETS" };
    spec.fieldLabel = "NAME:";
    spec.fieldText = current >= 0 && ! presets.isBuiltIn(current) ? presets.all()[static_cast<size_t>(current)].name
                                                                 : juce::String("MY ") + voiceNames[character];
    spec.buttons = { "SAVE", "CANCEL" };
    spec.onClose = [this](int button, const juce::String& text) { if (button == 0) savePresetAs(text); };
    showDialog(std::move(spec));
}

void TestUiView::savePresetAs(const juce::String& rawName)
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

void TestUiView::showOpenPreset()
{
    if (dialog != nullptr || chooser != nullptr) return;
    releaseQwertyNotes();
    PresetLibrary::folder().createDirectory();
    chooser = std::make_unique<juce::FileChooser>("OPEN PRESET", PresetLibrary::folder(), "*.phqpreset");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [safe = juce::Component::SafePointer<TestUiView>(this)](const juce::FileChooser& fc)
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

void TestUiView::toggleRecording()
{
    auto& recorder = processor.getRecorder();
    if (! recorder.isRecording())
    {
        if (dialog != nullptr) return;
        recordChannels = juce::jlimit(1, 2, processor.getTotalNumOutputChannels());
        if (! recorder.start(processor.getSampleRate(), recordChannels))
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
                   "~SAVED IN DOCUMENTS\\PHOQER\\RECORDINGS" };
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

void TestUiView::saveTake(const juce::String& rawName)
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

// =================================================================================== EDITOR
TestUiEditor::TestUiEditor(PhoqerAudioProcessor& owner) : AudioProcessorEditor(owner), view(owner, laf)
{
    setOpaque(true);
    setLookAndFeel(&laf);
    addAndMakeVisible(view);
    view.currentZoom = [this] { return zoom; };
    view.onZoom = [this](float z) { setZoom(z); };
    view.onFitToScreen = [this] { fitToScreen(); };
    view.onLayoutChanged = [this] { updateLimits(); setZoom(zoom); };

    setResizable(true, true);
    updateLimits();
    // Open at the saved zoom, shrunk if it no longer fits this screen (the saved choice is kept).
    const auto area = screenArea();
    const float fits = juce::jmin(static_cast<float>(area.getWidth()) / TestUiView::width,
                                  static_cast<float>(area.getHeight()) / static_cast<float>(view.baseHeight()));
    setZoom(juce::jmax(minZoom(), juce::jmin(UiSettings::get().zoom(), fits)));
    remembersZoom = true;
}

TestUiEditor::~TestUiEditor()
{
    setLookAndFeel(nullptr);
}

juce::Rectangle<int> TestUiEditor::screenArea() const
{
    const auto& displays = juce::Desktop::getInstance().getDisplays();
    const auto* display = isShowing() ? displays.getDisplayForRect(getScreenBounds()) : displays.getPrimaryDisplay();
    // Leave room for a host's window frame; the standalone window has none.
    return display != nullptr ? display->userArea.reduced(16, 40) : juce::Rectangle<int>(1280, 720);
}

void TestUiEditor::updateLimits()
{
    const int h = view.baseHeight();
    setResizeLimits(juce::roundToInt(TestUiView::width * minZoom()), juce::roundToInt(static_cast<float>(h) * minZoom()),
                    juce::roundToInt(TestUiView::width * maxZoom()), juce::roundToInt(static_cast<float>(h) * maxZoom()));
    if (auto* c = getConstrainer()) c->setFixedAspectRatio(static_cast<double>(TestUiView::width) / h);
}

void TestUiEditor::setZoom(float z)
{
    z = juce::jlimit(minZoom(), maxZoom(), z);
    setSize(juce::roundToInt(TestUiView::width * z), juce::roundToInt(static_cast<float>(view.baseHeight()) * z));
    zoom = z;
}

void TestUiEditor::fitToScreen()
{
    const auto area = screenArea();
    const float z = juce::jmin(static_cast<float>(area.getWidth()) / TestUiView::width,
                               static_cast<float>(area.getHeight()) / static_cast<float>(view.baseHeight()));
    setZoom(std::floor(z * 100.0f) / 100.0f);
}

void TestUiEditor::resized()
{
    const float z = static_cast<float>(getWidth()) / TestUiView::width;    // the height follows the fixed aspect ratio
    if (z <= 0.0f) return;
    zoom = z;
    view.setTransform(juce::AffineTransform::scale(z));
    if (remembersZoom) UiSettings::get().setZoom(z);
}
}
