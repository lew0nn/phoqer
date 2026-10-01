#include "TestUiEditor.h"

#include "TestUiLogo.h"
#include "TestUiStyle.h"

namespace phoqer::testui
{
namespace
{
// Window frames (editor coordinates).
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

// Header logo: the seal-sun icon at 18 px and the wordmark, both at 2 units per pixel like the rest of the UI.
constexpr int headerIconPixels = 18;
constexpr float logoPixel = 2.0f;

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
}

TestUiEditor::TestUiEditor(PhoqerAudioProcessor& owner)
    : AudioProcessorEditor(owner), processor(owner), scope(owner.getTelemetry()), meter(owner.getTelemetry()),
      piano(owner.getKeyboardState()),
      characterAttachment(parameterFor(owner.getParameters(), "character"),
                          [this](float value) { applyCharacter(juce::roundToInt(value)); }, nullptr)
{
    setOpaque(true);
    auto& state = processor.getParameters();
    lastCallSerial = processor.getTelemetry().getCallSerial();

    const int voiceOrder[] { 1, 0, 2 };
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
        spec.diameter = def.big ? 70.0f : isOutput ? 26.0f : 38.0f;
        spec.centreY = def.big ? 84.0f : 90.0f;
        spec.showNumbers = def.big;
        spec.layout = isOutput ? PixelKnob::Layout::output : PixelKnob::Layout::mixer;
        spec.format = isOutput ? decibels : percent;
        auto knob = std::make_unique<PixelKnob>(spec);
        addAndMakeVisible(*knob);
        sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, def.id, *knob));
        auto& p = parameterFor(state, def.id);
        knob->setDoubleClickReturnValue(true, p.convertFrom0to1(p.getDefaultValue()));
        knobs.push_back(std::move(knob));
    }

    for (auto* view : std::initializer_list<juce::Component*> { &seal, &scope, &meter, &menuBar, &piano })
        addAndMakeVisible(*view);
    const TitleButton98::Kind kinds[] { TitleButton98::Kind::minimise, TitleButton98::Kind::maximise, TitleButton98::Kind::close };
    for (size_t k = 0; k < titleButtons.size(); ++k)
        titleButtons[k] = std::make_unique<TitleButton98>(kinds[k]);    // shown once the window supplies controls
    titleButtons[0]->onClick = [this] { if (windowControls.minimise) windowControls.minimise(); };
    titleButtons[1]->onClick = [this] { if (windowControls.toggleFullscreen) windowControls.toggleFullscreen(); };
    titleButtons[2]->onClick = [this] { if (windowControls.close) windowControls.close(); };
    for (auto& b : titleButtons) addChildComponent(*b);
    heldNotes.fill(-1);

    // Key presses always land on the editor so the computer keyboard keeps playing after clicks.
    for (auto* child : getChildren())
    {
        child->setWantsKeyboardFocus(false);
        child->setMouseClickGrabsKeyboardFocus(false);
    }
    setWantsKeyboardFocus(true);
    menuBar.currentVoice = [this] { return character; };
    menuBar.onVoiceChosen = [this](int c) { setChoice("character", c); };

    characterAttachment.sendInitialUpdate();
    setSize(width, height);
    tick(juce::Time::getMillisecondCounterHiRes() * 0.001);
    startTimerHz(30);
}

TestUiEditor::~TestUiEditor()
{
    stopTimer();
    releaseQwertyNotes();
}

void TestUiEditor::releaseQwertyNotes()
{
    for (auto& note : heldNotes)
        if (note >= 0) { processor.getKeyboardState().noteOff(1, note, 0.0f); note = -1; }
}

bool TestUiEditor::keyStateChanged(bool)
{
    bool used = false;
    for (int i = 0; i < qwertyKeyCount; ++i)
    {
        auto& held = heldNotes[static_cast<size_t>(i)];
        const bool down = juce::KeyPress::isKeyCurrentlyDown(qwertyKeys[i]);
        if (down && held < 0)
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
        if (down && ! controlKeysDown[k])
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

void TestUiEditor::setWindowControls(WindowControls controls)
{
    windowControls = std::move(controls);
    titleButtons[0]->setVisible(windowControls.minimise != nullptr);
    titleButtons[1]->setVisible(windowControls.toggleFullscreen != nullptr);
    titleButtons[2]->setVisible(windowControls.close != nullptr);
    menuBar.onAudioSettings = windowControls.audioSettings;
    menuBar.onExit = windowControls.close;
}

bool TestUiEditor::onTitleBar(juce::Point<float> p) const
{
    return windowTitleBar(headWindow, headTitleHeight).contains(p);
}

// The PHOQER.EXE title bar moves the standalone window, and double-clicking it toggles fullscreen.
void TestUiEditor::mouseDown(const juce::MouseEvent& e)
{
    draggingWindow = windowControls.startDrag != nullptr && onTitleBar(e.position);
    if (draggingWindow) windowControls.startDrag(e);
}

void TestUiEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingWindow && windowControls.drag) windowControls.drag(e);
}

void TestUiEditor::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (windowControls.toggleFullscreen && onTitleBar(e.position)) windowControls.toggleFullscreen();
}

bool TestUiEditor::keyPressed(const juce::KeyPress& key)
{
    // Swallow the playing keys so the host or OS does not also act on them.
    const auto c = static_cast<char>(juce::CharacterFunctions::toLowerCase(static_cast<juce::juce_wchar>(key.getKeyCode())));
    return ! key.getModifiers().isCommandDown() && c != 0
        && (juce::String(qwertyKeys).containsChar(c) || juce::String("zxcv").containsChar(c));
}

void TestUiEditor::setChoice(const char* id, int index)
{
    auto& p = parameterFor(processor.getParameters(), id);
    p.beginChangeGesture();
    p.setValueNotifyingHost(p.convertTo0to1(static_cast<float>(index)));
    p.endChangeGesture();
}

void TestUiEditor::applyCharacter(int c)
{
    character = juce::jlimit(0, 2, c);
    logoIcon = renderSealSunIcon(character, headerIconPixels);
    logoWord = renderOutrunWordmark(character, false);
    for (auto& k : knobs) k->setCharacter(character);
    for (auto& b : modeButtons) b->setCharacter(character);
    scope.setCharacter(character);
    meter.setCharacter(character);
    piano.setCharacter(character);
    chrome = {};
    repaint();
}

void TestUiEditor::resized()
{
    const auto client = [](juce::Rectangle<float> r) { return windowClient(r); };
    const auto head = windowClient(headWindow, headTitleHeight);
    menuBar.setBounds(juce::Rectangle<float>(head.getX(), head.getY() - 2.0f, head.getWidth(), 17.0f).toNearestInt());
    const auto row = head.withTrimmedTop(17.0f);
    for (size_t slot = 0; slot < voiceTabs.size(); ++slot)
        voiceTabs[slot]->setBounds(juce::Rectangle<float>(row.getRight() - 4.0f - (3 - slot) * 94.0f + 4.0f, row.getCentreY() - 12.0f, 90.0f, 24.0f).toNearestInt());

    const auto tools = client(modeWindow);
    for (size_t m = 0; m < modeButtons.size(); ++m)
        modeButtons[m]->setBounds(juce::Rectangle<float>(tools.getX() + 2.0f, tools.getY() + 2.0f + m * 49.0f, tools.getWidth() - 4.0f, 46.0f).toNearestInt());

    seal.setBounds(client(sealWindow).withTrimmedBottom(18.0f).reduced(3.0f).toNearestInt());
    scope.setBounds(client(scopeWindow).withTrimmedBottom(18.0f).reduced(2.0f).toNearestInt());
    const auto out = client(outputWindow);
    meter.setBounds(juce::Rectangle<float>(out.getX() + 2.0f, out.getY() + 4.0f, 280.0f, 40.0f).toNearestInt());
    knobs[7]->setBounds(juce::Rectangle<float>(out.getRight() - 96.0f, out.getY(), 96.0f, out.getHeight()).toNearestInt());

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
}

void TestUiEditor::rebuildChrome(float scale)
{
    chromeScale = scale;
    chrome = juce::Image(juce::Image::RGB, juce::roundToInt(width * scale), juce::roundToInt(height * scale), true);
    juce::Graphics g(chrome);
    g.addTransform(juce::AffineTransform::scale(scale));
    const auto& pal = paletteFor(character);
    const auto title = pal.accent.darker(0.35f);
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

    // Header window: app logo (seal sun + Outrun wordmark); the voice tabs sit on the right.
    const auto head = window98(g, headWindow, "PHOQER.EXE - " + voice, title, true, headTitleHeight);
    const auto row = head.withTrimmedTop(17.0f);
    g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
    g.setOpacity(1.0f);
    const float iconSize = headerIconPixels * logoPixel;
    const juce::Rectangle<float> iconArea { row.getX() + 4.0f, std::round(row.getCentreY() - iconSize * 0.5f), iconSize, iconSize };
    g.drawImage(logoIcon, iconArea);
    const float wordW = logoWord.getWidth() * logoPixel, wordH = logoWord.getHeight() * logoPixel;
    g.drawImage(logoWord, { iconArea.getRight() + 8.0f, std::round(row.getCentreY() - wordH * 0.5f), wordW, wordH });

    window98(g, modeWindow, "MODE", title, false);
    const auto sealClient = window98(g, sealWindow, voice + ".BMP", title, true);
    sunken(g, sealClient.withTrimmedBottom(18.0f), juce::Colours::black);
    const auto scopeClient = window98(g, scopeWindow, "SCOPE.EXE", title, true);
    sunken(g, scopeClient.withTrimmedBottom(18.0f), juce::Colours::black);
    window98(g, outputWindow, "OUTPUT", title, false);
    const auto mix = window98(g, mixerWindow, "MIXER.EXE", title, false);
    g.setColour(win98::shadow);
    g.fillRect(mix.getX() + 541.0f, mix.getY() + 14.0f, 1.0f, mix.getHeight() - 28.0f);
    g.setColour(win98::light);
    g.fillRect(mix.getX() + 542.0f, mix.getY() + 14.0f, 1.0f, mix.getHeight() - 28.0f);
    const auto keys = window98(g, keysWindow, "KEYS.EXE", title, false);
    sunken(g, piano.getBounds().toFloat().expanded(2.0f), win98::dark);
    juce::ignoreUnused(keys);
}

void TestUiEditor::paint(juce::Graphics& g)
{
    // The static chrome is cached at the physical pixel scale it is drawn at.
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
    const auto keysClient = windowClient(keysWindow);
    statusBar(g, keysClient.withTrimmedTop(keysClient.getHeight() - 15.0f),
              { "TYPE A TO ; TO PLAY", "Z/X OCTAVE  A=" + noteName(qwertyBase), "C/V VELOCITY " + juce::String(qwertyVelocity) });
}

void TestUiEditor::timerCallback()
{
    tick(juce::Time::getMillisecondCounterHiRes() * 0.001);
}

void TestUiEditor::tick(double now)
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
}
