#include "TestUiEditor.h"

#include "TestUiStyle.h"

namespace phoqer::testui
{
namespace
{
// Window frames (editor coordinates).
const juce::Rectangle<float> headWindow { 6, 4, 748, 72 };
const juce::Rectangle<float> modeWindow { 6, 82, 86, 278 };
const juce::Rectangle<float> sealWindow { 98, 82, 262, 278 };
const juce::Rectangle<float> scopeWindow { 366, 82, 388, 194 };
const juce::Rectangle<float> outputWindow { 366, 282, 388, 78 };
const juce::Rectangle<float> mixerWindow { 6, 366, 748, 228 };

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
}

TestUiEditor::TestUiEditor(PhoqerAudioProcessor& owner)
    : AudioProcessorEditor(owner), processor(owner), scope(owner.getTelemetry()), meter(owner.getTelemetry()),
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

    for (auto* view : std::initializer_list<juce::Component*> { &seal, &scope, &meter, &menuBar, &taskbar })
        addAndMakeVisible(*view);
    menuBar.currentVoice = [this] { return character; };
    menuBar.onVoiceChosen = [this](int c) { setChoice("character", c); };

    characterAttachment.sendInitialUpdate();
    setSize(width, height);
    taskbar.tickClock();
    tick(juce::Time::getMillisecondCounterHiRes() * 0.001);
    startTimerHz(30);
}

TestUiEditor::~TestUiEditor() { stopTimer(); }

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
    for (auto& k : knobs) k->setCharacter(character);
    for (auto& b : modeButtons) b->setCharacter(character);
    scope.setCharacter(character);
    meter.setCharacter(character);
    taskbar.setCharacter(character);
    chrome = {};
    repaint();
}

void TestUiEditor::resized()
{
    const auto client = [](juce::Rectangle<float> r) { return windowClient(r); };
    const auto head = client(headWindow);
    menuBar.setBounds(juce::Rectangle<float>(head.getX(), head.getY() - 2.0f, head.getWidth(), 17.0f).toNearestInt());
    const auto row = head.withTrimmedTop(17.0f);
    for (size_t slot = 0; slot < voiceTabs.size(); ++slot)
        voiceTabs[slot]->setBounds(juce::Rectangle<float>(234.0f + slot * 94.0f, row.getY() + 3.0f, 90.0f, row.getHeight() - 6.0f).toNearestInt());

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
    taskbar.setBounds(0, height - 30, width, 30);
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

    // Header window: logo (RGB split), preset combo (disabled until presets exist).
    const auto head = window98(g, headWindow, "PHOQER.EXE - " + voice, title, true);
    const auto row = head.withTrimmedTop(17.0f);
    const std::pair<float, juce::Colour> split[] { { -2.0f, pal.accent.withAlpha(0.85f) }, { 2.0f, pal.secondary.withAlpha(0.85f) }, { 0.0f, juce::Colours::black } };
    for (const auto& [dx, colour] : split)
        drawText(g, "PHOQER", row.withWidth(200.0f).translated(6.0f + dx, 0.0f), logoFont(19.0f), colour);
    const juce::Rectangle<float> combo { 536.0f, row.getY() + 3.0f, 168.0f, row.getHeight() - 6.0f };
    sunken(g, combo);
    drawText(g, "001 INIT", combo.reduced(6.0f, 0.0f), pixelFont(10.0f), win98::shadow);
    const juce::Rectangle<float> drop { combo.getRight() - 18.0f, combo.getY() + 3.0f, 15.0f, combo.getHeight() - 6.0f };
    button98(g, drop, false);
    juce::Path tri;
    tri.addTriangle(drop.getCentreX() - 4, drop.getCentreY() - 2, drop.getCentreX() + 4, drop.getCentreY() - 2, drop.getCentreX(), drop.getCentreY() + 2);
    g.setColour(win98::shadow);
    g.fillPath(tri);
    for (int k = 0; k < 2; ++k)
    {
        const juce::Rectangle<float> b { 708.0f + k * 20.0f, combo.getY(), 18.0f, combo.getHeight() };
        button98(g, b, false);
        juce::Path arrow;
        const float cx = b.getCentreX(), cy = b.getCentreY();
        if (k == 0) arrow.addTriangle(cx + 2, cy - 4, cx + 2, cy + 4, cx - 2, cy);
        else arrow.addTriangle(cx - 2, cy - 4, cx - 2, cy + 4, cx + 2, cy);
        g.fillPath(arrow);
    }

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
    taskbar.setMidiActive(target.intensity > 0.02f);
    if ((frame % 15) == 0) taskbar.tickClock();
    repaint(windowClient(sealWindow).withTrimmedTop(windowClient(sealWindow).getHeight() - 15.0f).toNearestInt());
    repaint(windowClient(scopeWindow).withTrimmedTop(windowClient(scopeWindow).getHeight() - 15.0f).toNearestInt());
    ++frame;
}
}
