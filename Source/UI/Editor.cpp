// PhoqerView: the main window. Building it, layout, drawing, the 60 Hz tick, the keyboard and the mouse;
// and PhoqerEditor, the plugin window around it (zoom, screen fit).
#include "EditorShared.h"

namespace phoqer::ui
{
// ===================================================================================== VIEW
PhoqerView::PhoqerView(PhoqerAudioProcessor& owner, Win98LookAndFeel& laf)
    : processor(owner), lookAndFeel(laf), scope(owner.getTelemetry()), meter(owner.getTelemetry()),
      bpmField(parameterFor(owner.getParameters(), "standaloneTempo"), "BPM", 20, 400, 15.0f),
      wailBendPanel(parameterFor(owner.getParameters(), "wailBend"), parameterFor(owner.getParameters(), "wailHold")),
      piano(owner.getKeyboardState()),
      characterAttachment(parameterFor(owner.getParameters(), "character"),
                          [this](float) { applyCharacter(processor.getVoices().getVisualVoice()); }, nullptr),
      presets(owner),
      history(owner, [this] { return presets.loadedName()+"\n"+processor.getVoices().captureVoiceToken(); },
              [this](const juce::String& name) { presets.select(name.upToFirstOccurrenceOf("\n",false,false));processor.getVoices().restoreVoiceToken(name.fromFirstOccurrenceOf("\n",false,false));processor.getVoices().notifyVoiceChange(); })
{
    setOpaque(true);
    auto& state = processor.getParameters();
    lastCallSerial = processor.getTelemetry().getCallSerial();
    showKeys = UiSettings::get().showKeys();

    for (size_t slot = 0; slot < voiceTabs.size(); ++slot)
    {
        const int c=static_cast<int>(slot);
        voiceTabs[slot]=std::make_unique<VoiceSlot98>(c);
        voiceTabs[slot]->label=voiceNames[c];
        voiceTabs[slot]->setComponentID("voice"+juce::String(c));
        voiceTabs[slot]->onClick=[this,c]{if(voiceTabs[static_cast<size_t>(c)]->available)chooseVoice(c);else showVoiceBrowser();};
        addAndMakeVisible(*voiceTabs[slot]);
        voiceRows[slot]=std::make_unique<VoiceLibraryRow98>(c);
        voiceRows[slot]->setComponentID("voiceRow"+juce::String(c));
        voiceRows[slot]->onClick=[this,c]{browserVoice=c;voiceBrowser.repaint();refreshVoiceControls();};
        voiceBrowser.addAndMakeVisible(*voiceRows[slot]);
    }
    quickVoice.setComponentID("quickVoice");quickVoice.setWantsKeyboardFocus(false);quickVoice.setMouseClickGrabsKeyboardFocus(false);
    quickVoice.onClick=[this]
    {
        auto& tree=processor.getParameters().state;
        int mask=static_cast<int>(tree.getProperty("voiceQuickMask",15));
        const int bit=1<<browserVoice;
        mask=quickVoice.getToggleState()?mask|bit:mask&~bit;
        tree.setProperty("voiceQuickMask",mask,nullptr);
        // The quick bar shows its voices in the order they were put in it: one ticked goes to the end.
        if(quickVoice.getToggleState())
        {
            const auto digit=juce::String(browserVoice);
            UiSettings::get().setVoiceOrder(UiSettings::get().voiceOrder().removeCharacters(digit)+digit);
        }
        refreshVoiceControls();
    };
    voiceBrowser.addAndMakeVisible(quickVoice);
    browseVoices.setComponentID("browseVoices");browseVoices.setTooltip("BROWSE VOICES");
    browseVoices.onClick=[this]{showVoiceBrowser();};addAndMakeVisible(browseVoices);
    voiceBrowser.setComponentID("voiceBrowser");voiceBrowser.onClose=[this]{voiceBrowser.setVisible(false);};
    voiceBrowser.paintFrame=paintVoiceLibraryFrame;
    voiceBrowser.paintContent=[](juce::Graphics& g,juce::Rectangle<float>){paintVoiceLibraryContent(g);};
    tryOnlineButton.setComponentID("tryOnline");
    tryOnlineButton.onClick=[]{juce::URL("https://lewonn.com/phoqer#try").launchInDefaultBrowser();};     // play KYRAX in the browser demo
    useVoiceButton.setComponentID("useVoice");
    useVoiceButton.onClick=[this]{if(browserVoice==3&&!processor.getVoices().isVoiceUnlocked()){showEnterKey();return;}chooseVoice(browserVoice);voiceBrowser.setVisible(false);};
    useVoiceButton.primary=true;
    keyButton.setComponentID("keyButton");
    keyButton.onClick=[this]{if(processor.getVoices().isVoiceUnlocked())showRemoveKey();else juce::URL("https://lewonn.itch.io/kyrax").launchInDefaultBrowser();};
    loadVoiceButton.setComponentID("loadVoice");
    loadVoiceButton.onClick=[this]{chooseVoiceFile();};
    for(auto* button:{&tryOnlineButton,&loadVoiceButton,&useVoiceButton,&keyButton})voiceBrowser.addAndMakeVisible(*button);
    addChildComponent(voiceBrowser);
    for (size_t m = 0; m < modeButtons.size(); ++m)
    {
        modeButtons[m] = std::make_unique<ChoiceButton>(ChoiceButton::Kind::mode, static_cast<int>(m), modeNames[m], parameterFor(state, "behavior"));
        addAndMakeVisible(*modeButtons[m]);
    }
    modeButtons[3]->onReselect = [this] { showWailBend(); };
    modeButtons[3]->setTooltip("WAIL: CLICK AGAIN (OR RIGHT-CLICK) FOR WAIL BEND");
    wailBendPanel.onBeginEdit = [this] { releaseQwertyNotes(); };
    addChildComponent(wailBendPanel);

    struct KnobDef { const char* id; const char* name; const char* description; bool big; };
    const KnobDef defs[] { { "boom", "BOOM", "LOW BODY", true }, { "air", "AIR", "BREATH", true }, { "bark", "BARK", "GROWL", true },
                           { "space", "SPACE", "ROOM", true }, { "vowel", "VOWEL", "FORMANT", false }, { "detune", "DETUNE", "SPREAD", false },
                           { "tide", "TIDE", "MOTION", false }, { "glide", "GLIDE", "SLIDE", false }, { "output", "OUTPUT", "LEVEL", false } };
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
        spec.format = isOutput ? decibels : juce::String(def.id) == "glide" ? milliseconds : percent;
        auto knob = std::make_unique<PixelKnob>(spec);
        addAndMakeVisible(*knob);
        sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, def.id, *knob));
        auto& p = parameterFor(state, def.id);
        knob->setDoubleClickReturnValue(true, p.convertFrom0to1(p.getDefaultValue()));
        knobs.push_back(std::move(knob));
    }

    // YAP + NITRO
    yapButton.setComponentID("yap");
    nitroButton.setComponentID("nitro");
    yapAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state, "yap", yapButton);
    nitroAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state, "nitro", nitroButton);
    nitroButton.onRightClick = [this] { openNitro(); };
    addAndMakeVisible(yapButton);
    addAndMakeVisible(nitroButton);
    nitroKeys = std::make_unique<NitroKeys98>(std::array<juce::RangedAudioParameter*, 3> {
        &parameterFor(state, "nitroNote1"), &parameterFor(state, "nitroNote2"), &parameterFor(state, "nitroNote3") });
    nitroKeys->setComponentID("nitroKeys");
    nitroWindow.addAndMakeVisible(*nitroKeys);
    for (size_t k = 0; k < nitroLevels.size(); ++k)
    {
        const auto id = "nitroLevel" + juce::String(static_cast<int>(k) + 1);
        auto& slider = nitroLevels[k];
        slider.colour = nitroNoteColour(static_cast<int>(k));
        slider.setComponentID(id);
        nitroAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, id, slider));
        slider.onValueChange = [this] { nitroWindow.repaint(); };
        nitroWindow.addChildComponent(slider);
        nitroWatch.push_back(std::make_unique<juce::ParameterAttachment>(parameterFor(state, ("nitroNote" + juce::String(static_cast<int>(k) + 1)).toRawUTF8()),
                                                                        [this](float) { layoutNitro(); nitroKeys->repaint(); nitroWindow.repaint(); }, nullptr));
    }
    {
        struct NitroKnob { const char* id; const char* name; std::function<juce::String(double)> format; };
        const NitroKnob nitroDefs[] { { "nitroWidth", "WIDTH", percent },
                                 { "nitroStrum", "STRUM", [](double v) { return juce::String(juce::roundToInt(v)) + " MS"; } },
                                 { "nitroDetune", "DETUNE", [](double v) { return juce::String(v, v < 10.0 ? 1 : 0) + " CT"; } } };
        for (const auto& def : nitroDefs)
        {
            PixelKnob::Spec spec;
            spec.name = def.name;
            spec.radius = 12.0f;
            spec.centreY = 20.0f;     // low enough that the dial's ring is not cut off at the top
            spec.big = false;
            spec.nameY = 37.0f;
            spec.boxY = 48.0f;
            spec.boxH = 15.0f;
            spec.valueSize = 10.0f;
            spec.format = def.format;
            auto knob = std::make_unique<PixelKnob>(spec);
            knob->setComponentID(def.id);
            nitroAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, def.id, *knob));
            auto& p = parameterFor(state, def.id);
            knob->setDoubleClickReturnValue(true, p.convertFrom0to1(p.getDefaultValue()));
            nitroWindow.addAndMakeVisible(*knob);
            nitroKnobs.push_back(std::move(knob));
        }
    }
    for (int i = 0; i < static_cast<int>(std::size(nitroStacks)); ++i) stackBox.addItem(nitroStacks[i].name, i + 1);
    stackBox.setTextWhenNothingSelected("CUSTOM");
    stackBox.setComponentID("nitroStack");
    stackBox.onChange = [this]
    {
        const int i = stackBox.getSelectedId() - 1;
        if (! juce::isPositiveAndBelow(i, static_cast<int>(std::size(nitroStacks)))) return;
        for (int k = 0; k < 3; ++k)
        {
            auto& p = parameterFor(processor.getParameters(), ("nitroNote" + juce::String(k + 1)).toRawUTF8());
            p.beginChangeGesture();
            p.setValueNotifyingHost(p.convertTo0to1(static_cast<float>(nitroStacks[i].notes[k])));
            p.endChangeGesture();
        }
    };
    nitroWindow.addAndMakeVisible(stackBox);
    nitroOk.setComponentID("nitroOk");
    nitroOk.primary = true;
    nitroOk.onClick = [this] { nitroWindow.setVisible(false); };
    nitroWindow.addAndMakeVisible(nitroOk);
    nitroWindow.setComponentID("nitroWindow");
    nitroWindow.paintContent = [this](juce::Graphics& g, juce::Rectangle<float> c)
    {
        auto& state = processor.getParameters();
        const auto keys = nitroKeys->getBounds().toFloat();
        const float w = nitroKeys->whiteWidth();
        for (const auto [offset, text] : { std::pair { -12, "-12" }, { 0, "ROOT" }, { 12, "+12" }, { 24, "+24" } })
            drawText(g, text, { keys.getX() + nitroKeys->keyX(offset) + w * 0.5f - 15.0f, keys.getBottom() + 2.0f, 30.0f, 8.0f },
                     pixelFont(8.0f), win98::shadow, juce::Justification::centred);
        int row = 0;
        for (int k = 0; k < 3; ++k)
        {
            const int offset = juce::roundToInt(parameterFor(state, ("nitroNote" + juce::String(k + 1)).toRawUTF8()).convertFrom0to1(
                parameterFor(state, ("nitroNote" + juce::String(k + 1)).toRawUTF8()).getValue()));
            if (offset == 0) continue;
            const juce::Rectangle<float> r { c.getX() + nitroMargin, c.getY() + nitroRowsY + row * nitroRowGap, 30.0f, 16.0f };
            g.setColour(nitroNoteColour(k));
            g.fillRect(r);
            g.setColour(juce::Colours::black);
            g.drawRect(r, 1.0f);
            drawText(g, (offset > 0 ? "+" : "") + juce::String(offset), r, pixelFont(10.0f, true), juce::Colours::black, juce::Justification::centred);
            const auto box = juce::Rectangle<float>(c.getRight() - nitroMargin - 34.0f, r.getY(), 34.0f, 16.0f);
            sunken(g, box);
            drawText(g, juce::String(juce::roundToInt(nitroLevels[static_cast<size_t>(k)].getValue() * 100.0)) + "%", box.withTrimmedTop(3.0f),
                     pixelFont(10.0f, true), paletteFor(character).accent.darker(0.6f), juce::Justification::centred);
            ++row;
        }
        if (row < 3)
        {
            const juce::Rectangle<float> r { c.getX() + nitroMargin, c.getY() + nitroRowsY + row * nitroRowGap, c.getWidth() - 2.0f * nitroMargin, 16.0f };
            g.setColour(win98::shadow);
            g.drawRect(r, 1.0f);
            drawText(g, "+ ADD A NOTE: TAP A KEY", r, pixelFont(8.0f), win98::shadow, juce::Justification::centred);
        }
        drawText(g, "STACK", { c.getX() + nitroMargin, c.getBottom() - 28.0f, 40.0f, 16.0f }, pixelFont(10.0f, true), juce::Colours::black);
    };
    addChildComponent(nitroWindow);
    for (auto* child : nitroWindow.getChildren())
    {
        child->setWantsKeyboardFocus(false);
        child->setMouseClickGrabsKeyboardFocus(false);
    }

    // ARP, between YAP and NITRO. Right-click it for ARP.EXE.
    arpButton.setComponentID("arp");
    arpAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state, "arp", arpButton);
    arpButton.onRightClick = [this] { openArp(); };
    addAndMakeVisible(arpButton);
    arpSteps.setComponentID("arpSteps");
    arpSteps.get = [this](int i) { return processor.getArpLane(i); };
    arpSteps.set = [this](int i, phoqer::ArpLaneStep step) { processor.setArpLane(i, step); };
    arpSteps.edited = [this] { processor.arpLanesEdited(); };
    arpSteps.playing = [this] { return processor.getPlayingArpLane(); };
    arpWindow.addAndMakeVisible(arpSteps);
    for (int i = 0; i < 2; ++i)
    {
        auto& b = arpLaneModes[static_cast<size_t>(i)];
        b = std::make_unique<SegmentButton98>(i == 0 ? "VEL" : "GATE");
        b->setComponentID(i == 0 ? "arpLaneVel" : "arpLaneGate");
        b->setTooltip(i == 0 ? "VEL: HOW LOUD EACH STEP PLAYS. DRAG TO DRAW, RIGHT-CLICK FOR A REST"
                             : "GATE: HOW LONG EACH STEP PLAYS, TIMES THE GATE KNOB. DRAG TO DRAW, RIGHT-CLICK FOR A REST");
        b->onPick = [this, i] { arpSteps.gateMode = i == 1; refreshArp(); };
        arpWindow.addAndMakeVisible(*b);
    }
    const auto setArp = [this](const char* id, float value)
    {
        auto& p = parameterFor(processor.getParameters(), id);
        p.beginChangeGesture();
        p.setValueNotifyingHost(p.convertTo0to1(value));
        p.endChangeGesture();
        refreshArp();     // the buttons show the pick at once, not on the next update
    };
    const char* const rateNames[] { "1/4", "1/8", "1/16", "1/32", "1/8T" };
    for (int i = 0; i < 5; ++i)
    {
        auto& b = arpRates[static_cast<size_t>(i)];
        b = std::make_unique<SegmentButton98>(rateNames[i]);
        b->setComponentID("arpRate" + juce::String(i));
        b->onPick = [setArp, i] { setArp("arpRate", static_cast<float>(i)); };
        arpWindow.addAndMakeVisible(*b);
    }
    for (int i = 0; i < 2; ++i)
    {
        auto& b = arpHolds[static_cast<size_t>(i)];
        b = std::make_unique<SegmentButton98>(i == 0 ? "OFF" : "ON");
        b->setComponentID(i == 0 ? "arpHoldOff" : "arpHoldOn");
        b->setTooltip("HOLD: KEEPS PLAYING AFTER YOU LET GO. A NEW CHORD REPLACES IT");
        b->onPick = [setArp, i] { setArp("arpHold", static_cast<float>(i)); };
        arpWindow.addAndMakeVisible(*b);
    }
    {
        struct ArpKnob { const char* id; const char* name; std::function<juce::String(double)> format; };
        const ArpKnob arpDefs[] { { "arpGate", "GATE", percent }, { "arpSwing", "SWING", percent },
                                  { "arpOctaves", "OCTAVES", [](double v) { return juce::String(juce::roundToInt(v)) + " OCT"; } } };
        for (const auto& def : arpDefs)
        {
            PixelKnob::Spec spec;
            spec.name = def.name;
            spec.radius = 12.0f;
            spec.centreY = 20.0f;
            spec.big = false;
            spec.nameY = 35.5f;     // 6 px from the dial to its name and from the name to its box
            spec.boxY = 49.0f;
            spec.boxH = 15.0f;
            spec.valueSize = 10.0f;
            spec.format = def.format;
            auto knob = std::make_unique<PixelKnob>(spec);
            knob->setComponentID(def.id);
            arpAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, def.id, *knob));
            auto& p = parameterFor(state, def.id);
            knob->setDoubleClickReturnValue(true, p.convertFrom0to1(p.getDefaultValue()));
            arpWindow.addAndMakeVisible(*knob);
            arpKnobs.push_back(std::move(knob));
        }
    }
    for (const auto* name : { "UP", "DOWN", "UP + DOWN", "RANDOM", "AS PLAYED" }) arpPatternBox.addItem(name, arpPatternBox.getNumItems() + 1);
    arpPatternBox.setComponentID("arpPattern");
    arpPatternAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, "arpPattern", arpPatternBox);
    arpWindow.addAndMakeVisible(arpPatternBox);
    for (const auto* id : { "arpRate", "arpHold", "arpPattern", "arpOctaves" })
        arpWatch.push_back(std::make_unique<juce::ParameterAttachment>(parameterFor(state, id), [this](float) { refreshArp(); }, nullptr));
    arpOk.setComponentID("arpOk");
    arpOk.primary = true;
    arpOk.onClick = [this] { arpWindow.setVisible(false); };
    arpWindow.addAndMakeVisible(arpOk);
    arpWindow.setComponentID("arpWindow");
    arpWindow.paintContent = [this](juce::Graphics& g, juce::Rectangle<float> c)
    {
        const auto font = pixelFont(10.0f, true);
        drawText(g, "RATE", { c.getX() + nitroMargin, arpRates[0]->getBounds().toFloat().getY(), 44.0f, 17.0f }, font, juce::Colours::black);
        drawText(g, "HOLD", { c.getX() + nitroMargin, arpHolds[0]->getBounds().toFloat().getY(), 44.0f, 17.0f }, font, juce::Colours::black);
        drawText(g, "PATTERN", { c.getX() + nitroMargin, c.getBottom() - 28.0f, 56.0f, 16.0f }, font, juce::Colours::black);
    };
    addChildComponent(arpWindow);
    for (auto* child : arpWindow.getChildren())
    {
        child->setWantsKeyboardFocus(false);
        child->setMouseClickGrabsKeyboardFocus(false);
    }
    refreshArp();
    setupUpdates();
    diceButton.setComponentID("dice");
    diceButton.onClick = [this] { randomizeKnobs(); };
    addAndMakeVisible(diceButton);

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
    for (auto* c : std::initializer_list<juce::Component*> { &bpmField, &metronomeButton, &metronomeVolume, &accentBox })
        metroWindow.addAndMakeVisible(c);
    metroWindow.setComponentID("metroWindow");
    addChildComponent(metroWindow);
    addChildComponent(barMetronome);
    addChildComponent(barTempo);
    barMetronome.setComponentID("barMetronome");
    barTempo.setComponentID("metroOpen");
    barMetronome.setTooltip("METRONOME ON / OFF (NOT IN WAV TAKES)");
    barTempo.setTooltip("BPM: DRAG UP / DOWN TO CHANGE. CLICK FOR METRO.EXE (VOLUME, BEATS, SOUND, COUNT-IN)");
    barTempo.setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    barTempo.onDrag = [this](int steps)
    {
        auto& p = parameterFor(processor.getParameters(), "standaloneTempo");
        p.beginChangeGesture();
        p.setValueNotifyingHost(p.convertTo0to1(p.convertFrom0to1(p.getValue()) + static_cast<float>(steps)));
        p.endChangeGesture();
    };
    barMetronome.beats = [this] { return processor.getMetronomeBeats(); };
    barMetronomeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state, "metronomeEnabled", barMetronome);
    barTempo.onClick = [this] { setMetroOpen(! metroWindow.isVisible()); };
    metroWindow.onClose = [this] { setMetroOpen(false); };
    metroWindow.onMoved = [this]
    {
        processor.getParameters().state.setProperty("metroX", metroWindow.getX(), nullptr);
        processor.getParameters().state.setProperty("metroY", metroWindow.getY(), nullptr);
    };
    metroWindow.paintContent = [this](juce::Graphics& g, juce::Rectangle<float> m)
    {
        const char* labels[] { "VOLUME", "BEATS", nullptr, "SOUND", "COUNT-IN" };
        for (int i = 0; i < 5; ++i)
            if (labels[i] != nullptr)
                drawText(g, labels[i], { m.getX(), m.getY() + 42.0f + i * 19.0f, 54.0f, 16.0f }, pixelFont(10.0f, true), juce::Colours::black);
        drawText(g, juce::String(juce::roundToInt(metronomeVolume.getValue())) + "%", { m.getRight() - 24.0f, m.getY() + 42.0f, 24.0f, 16.0f },
                 pixelFont(10.0f, true), juce::Colours::black, juce::Justification::centredRight);
        // Beat lights under the BPM glass: the current beat of the bar lights up while the click runs.
        const int n = processor.getMetronomeBeatsPerBar();
        const float x0 = m.getX() + 28.0f, w = 76.0f, gap = 3.0f, each = (w - gap * (n - 1)) / n;
        const auto& pal = paletteFor(character);
        for (int i = 0; i < n; ++i)
        {
            g.setColour(i == shownBeatLight ? pal.accent : pal.accent.withMultipliedBrightness(0.3f));
            g.fillRect(juce::Rectangle<float>(x0 + i * (each + gap), m.getY() + 31.0f, each, 4.0f).toNearestInt().toFloat());
        }
    };
    volumeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "metronomeVolume", metronomeVolume);
    accentAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state, "metronomeAccent", accentBox);
    metronomeVolume.onValueChange = [this] { metroWindow.repaint(); };
    for (auto [box, id] : { std::pair { &beatsBox, "metronomeBeats" }, { &soundBox, "metronomeSound" }, { &countInBox, "countIn" } })
    {
        box->addItemList(dynamic_cast<juce::AudioParameterChoice&>(*state.getParameter(id)).choices, 1);
        box->setComponentID(id);
        metroWindow.addAndMakeVisible(*box);
        metroBoxAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, id, *box));
    }
    metronomeVolume.setComponentID("metronomeVolume");
    accentBox.setComponentID("metronomeAccent");
    metronomeButton.setTooltip("METRONOME ON / OFF (FOLLOWS BPM; NOT IN WAV TAKES)");
    countInBox.setTooltip("CLICKS BEFORE REC STARTS RECORDING");
    bpmField.setComponentID("tempo");
    metronomeButton.setComponentID("metronome");
    metronomeButton.beats = [this] { return processor.getMetronomeBeats(); };
    metronomeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state, "metronomeEnabled", metronomeButton);
    bpmField.onBeginEdit = [this] { releaseQwertyNotes(); };

    addChildComponent(fish);
    addChildComponent(blueScreen);
    blueScreen.onDismiss = [this] { rebootSeal(); };
    // Dropped on the seal, the fish is eaten: a gulp and a squeak, a grin, and another fish later.
    fish.onDrop = [this](juce::Point<int> at)
    {
        if (! seal.getBounds().contains(at)) return false;
        feedFish();
        return true;
    };

    // Key presses always land on the view so the computer keyboard keeps playing after clicks.
    for (auto* child : getChildren())
    {
        child->setWantsKeyboardFocus(false);
        child->setMouseClickGrabsKeyboardFocus(false);
    }
    setWantsKeyboardFocus(true);

    characterAttachment.sendInitialUpdate();
    refreshVoiceControls();
    setSize(width, baseHeight());
    tick(juce::Time::getMillisecondCounterHiRes() * 0.001);
    startTimerHz(30);
}

PhoqerView::~PhoqerView()
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

void PhoqerView::releaseQwertyNotes()
{
    for (auto& note : heldNotes)
        if (note >= 0) { processor.getKeyboardState().noteOff(1, note, 0.0f); note = -1; }
}

bool PhoqerView::keyStateChanged(bool)
{
    // The computer-key piano is the standalone's: in a DAW the host's own typing keyboard plays PHOQER.
    if (! isStandalone()) return false;
    // No notes while a box is open (its text field gets the typing) or while Ctrl / Alt is held for a shortcut.
    if (dialog != nullptr || blueScreen.isVisible()) { releaseQwertyNotes(); return false; }
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
            if (k == 4) toggleMono();
            else if (k < 2) qwertyBase = juce::jlimit(lowestBase, highestBase, qwertyBase + (k == 0 ? -12 : 12));
            else qwertyVelocity = juce::jlimit(velocityStep, 127, qwertyVelocity + (k == 2 ? -velocityStep : velocityStep));
            piano.setQwertyBase(qwertyBase);
            repaint(windowClient(keysWindow).toNearestInt());
            used = true;
        }
        controlKeysDown[k] = down;
    }
    return used;
}

void PhoqerView::setWindowControls(WindowControls controls)
{
    windowControls = std::move(controls);
    titleButtons[0]->setVisible(windowControls.minimise != nullptr);
    titleButtons[1]->setVisible(windowControls.toggleFullscreen != nullptr);
    titleButtons[2]->setVisible(windowControls.close != nullptr);
    recordControl.setVisible(isStandalone());
    barMetronome.setVisible(isStandalone());
    barTempo.setVisible(isStandalone());
    piano.setQwertyLabels(isStandalone());          // the letters on the keys are the standalone's computer-key piano
    setMetroOpen(static_cast<bool>(processor.getParameters().state.getProperty("metroOpen", false)));
    chrome = {};                                    // the transport's group box is standalone only
    resized();
}

bool PhoqerView::onTitleBar(juce::Point<float> p) const
{
    return windowTitleBar(headWindow, headTitleHeight).contains(p);
}

juce::Rectangle<float> PhoqerView::keysBar() const
{
    const auto keysClient = windowClient(keysWindow);
    return keysClient.withTrimmedTop(keysClient.getHeight() - 15.0f);
}

juce::Rectangle<float> PhoqerView::monoArea() const     // the last of the bar's fields, as paint() splits it
{
    const auto bar = keysBar();
    const float each = bar.getWidth() * 0.25f;
    return { bar.getRight() - each, bar.getY(), each - 3.0f, bar.getHeight() };
}

juce::String PhoqerView::getTooltip()
{
    return showKeys && isStandalone() && monoArea().contains(getMouseXYRelative().toFloat())
        ? "MONO: ONE NOTE AT A TIME. POLY: CHORDS. CLICK OR PRESS M" : juce::String();
}

// The PHOQER.EXE title bar moves the standalone window, and double-clicking it toggles fullscreen.
void PhoqerView::mouseDown(const juce::MouseEvent& e)
{
    if (updateNotice().isNotEmpty() && noticeArea().contains(e.position)) { openUpdateFromNotice(); return; }     // UPDATE ... READY
    if (showKeys && isStandalone() && monoArea().contains(e.position)) { toggleMono(); return; }
    draggingWindow = windowControls.startDrag != nullptr && onTitleBar(e.position);
    if (draggingWindow) windowControls.startDrag(e);
}

void PhoqerView::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingWindow && windowControls.drag) windowControls.drag(e);
}

void PhoqerView::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (windowControls.toggleFullscreen && onTitleBar(e.position)) windowControls.toggleFullscreen();
    else if (logoBounds().translated(logoSlide, 0.0f).contains(e.position)) fishOut = ! fishOut;    // the secret
}

// The seal eats the fish: a gulp, then it sings the fish (watch SCOPE.EXE), grins, and a new fish
// comes out from behind the wordmark a few seconds later.
void PhoqerView::feedFish()
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    std::rotate(lastFed.begin(), lastFed.begin() + 1, lastFed.end());
    lastFed.back() = now;
    if (lastFed.front() > 0.0 && now - lastFed.front() < 30.0)
    {
        // Five fish in 30 seconds: a belch, and SEAL.SYS goes down.
        processor.feedSeal(true);
        crashAt = now + 0.5;
        fishBackAt = now + 1.0e9;
        return;
    }
    processor.feedSeal();
    scope.showSpectrogram(5.3);     // the fish draws in as it sings, then scrolls out to the left
    happyFrom = now;
    happyUntil = now + 3.0;
    fishBackAt = now + 3.0;
}

// Any key or click on the blue screen: back to normal, the wordmark closed, the count from zero.
void PhoqerView::rebootSeal()
{
    blueScreen.setVisible(false);
    lastFed = {};
    crashAt = 0.0;
    fishBackAt = 0.0;
    fishOut = false;
    grabKeyboardFocus();
}

juce::Rectangle<float> PhoqerView::logoBounds() const
{
    const auto row = windowClient(headWindow, headTitleHeight).withTrimmedTop(headerToolsHeight);
    const float wordW = logoWord.getWidth() * logoWordPixel, wordH = logoWord.getHeight() * logoWordPixel;
    // The 3D extrusion hangs below the letters, so centre the letters, not the whole image.
    constexpr float extrusionDrop = 2.0f;
    return { row.getX() + logoWordInset, std::round(row.getCentreY() - wordH * 0.5f + extrusionDrop), wordW, wordH };
}

float PhoqerView::fishSlide() const
{
    return juce::jmin(static_cast<float>(fish.getWidth() + 10), presetBoxX - 6.0f - logoBounds().getRight());
}

bool PhoqerView::keyPressed(const juce::KeyPress& key)
{
    if (blueScreen.isVisible()) { rebootSeal(); return true; }
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
    // Swallow the playing keys so the OS does not also act on them (standalone only; a DAW keeps its keys).
    return isStandalone() && ! key.getModifiers().isCommandDown() && ! key.getModifiers().isAltDown() && c != 0
        && (juce::String(qwertyKeys).containsChar(c) || juce::String("zxcvm").containsChar(c));
}

void PhoqerView::setChoice(const char* id, int index)
{
    auto& p = parameterFor(processor.getParameters(), id);
    p.beginChangeGesture();
    p.setValueNotifyingHost(p.convertTo0to1(static_cast<float>(index)));
    p.endChangeGesture();
}

void PhoqerView::applyCharacter(int c)
{
    character = juce::jlimit(0, 3, c);
    lookAndFeel.setCharacter(character);
    logoWord = renderOutrunWordmark(character, false);
    fish.setCharacter(character);
    for (auto& k : knobs) k->setCharacter(character);
    for (auto& b : modeButtons) b->setCharacter(character);
    wailBendPanel.setCharacter(character);
    scope.setCharacter(character);
    bpmField.setCharacter(character);
    metronomeButton.setCharacter(character);
    barMetronome.setCharacter(character);
    barTempo.setCharacter(character);
    metroWindow.setCharacter(character);
    voiceBrowser.setCharacter(character);
    meter.setCharacter(character);
    piano.setCharacter(character);
    const bool kyrax = character == 3;
    for (auto* b : { &yapButton, &arpButton, &nitroButton }) { b->setEnabled(kyrax); b->setVisible(kyrax); }
    yapButton.setTooltip("YAP: KYRAX TALKS IN 8THS AT THE TEMPO");
    arpButton.setTooltip("ARP: PLAYS THE HELD KEYS ONE BY ONE AT THE TEMPO. RIGHT-CLICK FOR ARP.EXE");
    nitroButton.setTooltip("NITRO: EXTRA NOTES ON EVERY KEY. RIGHT-CLICK FOR NITRO.EXE");
    if (! kyrax) { nitroWindow.setVisible(false); arpWindow.setVisible(false); }
    nitroWindow.setCharacter(character);
    arpWindow.setCharacter(character);
    updateWindow.setCharacter(character);
    for (auto& k : nitroKnobs) k->setCharacter(character);
    for (auto& k : arpKnobs) k->setCharacter(character);
    diceButton.setCharacter(character);
    chrome = {};
    repaint();
}

juce::Rectangle<float> PhoqerView::nitroBoxArea() const
{
    const auto mix = windowClient(mixerWindow);
    return { mix.getX() + 516.0f, mix.getY() + 174.0f, 214.0f, 35.0f };     // controls 8 below the top line, 7 above the bottom one
}

void PhoqerView::openNitro()
{
    if (! nitroButton.isEnabled()) return;
    layoutNitro();
    nitroWindow.setVisible(true);
    nitroWindow.toFront(false);
}

// NITRO.EXE: the keyboard, a row for each note in use (then one empty row), the three dials, STACK and OK.
void PhoqerView::layoutNitro()
{
    if (nitroWindow.getWidth() == 0)
    {
        const auto box = nitroBoxArea();
        nitroWindow.setBounds(juce::Rectangle<float>(box.getRight() - nitroWindowW, box.getY() - nitroWindowH - 4.0f, nitroWindowW, nitroWindowH)
                                  .toNearestInt().constrainedWithin(getLocalBounds()));
    }
    const auto c = nitroWindow.clientArea();
    nitroKeys->setBounds(juce::Rectangle<float>(c.getX() + nitroMargin, c.getY() + nitroKeysY, c.getWidth() - 2.0f * nitroMargin, nitroKeysH).toNearestInt());
    auto& state = processor.getParameters();
    int row = 0;
    std::array<int, 3> notes {};
    for (int k = 0; k < 3; ++k)
    {
        auto& p = parameterFor(state, ("nitroNote" + juce::String(k + 1)).toRawUTF8());
        notes[static_cast<size_t>(k)] = juce::roundToInt(p.convertFrom0to1(p.getValue()));
        auto& slider = nitroLevels[static_cast<size_t>(k)];
        slider.setVisible(notes[static_cast<size_t>(k)] != 0);
        if (notes[static_cast<size_t>(k)] == 0) continue;
        slider.setBounds(juce::Rectangle<float>(c.getX() + nitroMargin + 36.0f, c.getY() + nitroRowsY + row * nitroRowGap, c.getWidth() - 2.0f * nitroMargin - 78.0f, 16.0f).toNearestInt());
        ++row;
    }
    const float cell = (c.getWidth() - 2.0f * nitroMargin) / 3.0f;
    for (size_t k = 0; k < nitroKnobs.size(); ++k)
        nitroKnobs[k]->setBounds(juce::Rectangle<float>(c.getX() + nitroMargin + k * cell, c.getY() + nitroDialsY, cell, nitroDialH).toNearestInt());
    stackBox.setBounds(juce::Rectangle<float>(c.getX() + nitroMargin + 40.0f, c.getBottom() - 28.0f, 112.0f, 16.0f).toNearestInt());
    nitroOk.setBounds(juce::Rectangle<float>(c.getRight() - nitroMargin - 56.0f, c.getBottom() - 29.0f, 56.0f, 18.0f).toNearestInt());
    int match = 0;
    for (int i = 0; i < static_cast<int>(std::size(nitroStacks)); ++i)
    {
        std::array<int, 3> a { nitroStacks[i].notes[0], nitroStacks[i].notes[1], nitroStacks[i].notes[2] }, b = notes;
        std::sort(a.begin(), a.end());
        std::sort(b.begin(), b.end());
        if (a == b) { match = i + 1; break; }
    }
    stackBox.setSelectedId(match, juce::dontSendNotification);
}

void PhoqerView::openArp()
{
    if (! arpButton.isEnabled()) return;
    layoutArp();
    arpWindow.setVisible(true);
    arpWindow.toFront(false);
}

// ARP.EXE, NITRO.EXE's size: the step lanes and their VEL | GATE switch, RATE, HOLD, the three dials, PATTERN and OK.
void PhoqerView::layoutArp()
{
    if (arpWindow.getWidth() == 0)
    {
        const auto box = nitroBoxArea();
        arpWindow.setBounds(juce::Rectangle<float>(box.getRight() - nitroWindowW, box.getY() - nitroWindowH - 4.0f, nitroWindowW, nitroWindowH)
                                .toNearestInt().constrainedWithin(getLocalBounds()));
    }
    const auto c = arpWindow.clientArea();
    const float x0 = c.getX() + nitroMargin, w = c.getWidth() - 2.0f * nitroMargin;
    arpSteps.setBounds(juce::Rectangle<float>(x0, c.getY() + 8.0f, w, 46.0f).toNearestInt());
    const float bx = x0 + 46.0f, bw = std::floor((x0 + w - bx - 12.0f) / 5.0f);
    for (size_t i = 0; i < arpLaneModes.size(); ++i)     // VEL | GATE: right under the lanes, flush with their right edge
        arpLaneModes[i]->setBounds(juce::Rectangle<float>(x0 + w - 2.0f * bw - 3.0f + static_cast<float>(i) * (bw + 3.0f), c.getY() + 58.0f, bw, 15.0f).toNearestInt());
    for (size_t i = 0; i < arpRates.size(); ++i)
        arpRates[i]->setBounds(juce::Rectangle<float>(bx + static_cast<float>(i) * (bw + 3.0f), c.getY() + 82.0f, bw, 17.0f).toNearestInt());
    for (size_t i = 0; i < arpHolds.size(); ++i)
        arpHolds[i]->setBounds(juce::Rectangle<float>(bx + static_cast<float>(i) * (bw + 3.0f), c.getY() + 105.0f, bw, 17.0f).toNearestInt());
    const float cell = w / 3.0f;
    for (size_t k = 0; k < arpKnobs.size(); ++k)
        arpKnobs[k]->setBounds(juce::Rectangle<float>(x0 + static_cast<float>(k) * cell, c.getY() + 128.0f, cell, 72.0f).toNearestInt());
    arpPatternBox.setBounds(juce::Rectangle<float>(x0 + 58.0f, c.getBottom() - 28.0f, 104.0f, 16.0f).toNearestInt());
    arpOk.setBounds(juce::Rectangle<float>(c.getRight() - nitroMargin - 56.0f, c.getBottom() - 29.0f, 56.0f, 18.0f).toNearestInt());
}

void PhoqerView::refreshArp()
{
    auto& state = processor.getParameters();
    const int rate = juce::roundToInt(state.getRawParameterValue("arpRate")->load());
    const bool hold = state.getRawParameterValue("arpHold")->load() >= 0.5f;
    for (size_t i = 0; i < arpRates.size(); ++i)
        if (arpRates[i] != nullptr) arpRates[i]->setToggleState(static_cast<int>(i) == rate, juce::dontSendNotification);
    if (arpHolds[0] != nullptr) arpHolds[0]->setToggleState(! hold, juce::dontSendNotification);
    if (arpHolds[1] != nullptr) arpHolds[1]->setToggleState(hold, juce::dontSendNotification);
    for (size_t i = 0; i < arpLaneModes.size(); ++i)
        if (arpLaneModes[i] != nullptr) arpLaneModes[i]->setToggleState((i == 1) == arpSteps.gateMode, juce::dontSendNotification);
    arpSteps.repaint();
}

juce::Rectangle<float> PhoqerView::transportArea() const
{
    const auto mix = windowClient(mixerWindow);
    return { mix.getX() + 522.0f, mix.getY() + 14.0f, 196.0f, 24.0f };     // centred over VOWEL..GLIDE
}

void PhoqerView::setMetroOpen(bool open)
{
    metroWindow.setVisible(open && isStandalone());
    processor.getParameters().state.setProperty("metroOpen", open, nullptr);
    if (metroWindow.isVisible()) metroWindow.toFront(false);
}

void PhoqerView::resized()
{
    const auto client = [](juce::Rectangle<float> r) { return windowClient(r); };
    const auto head = windowClient(headWindow, headTitleHeight);
    menuBar.setBounds(juce::Rectangle<float>(head.getX(), head.getY() - 2.0f, 452.0f, headerToolsHeight).toNearestInt());
    const auto row = head.withTrimmedTop(headerToolsHeight);
    voiceControlsKey = {};     // the quick bar lays its tabs out in refreshVoiceControls
    refreshVoiceControls();
    browseVoices.setBounds(718,62,26,22);
    if(voiceBrowser.getWidth()==0)voiceBrowser.setBounds(506,96,238,298);
    for(size_t i=0;i<4;++i)voiceRows[i]->setBounds(13,48+static_cast<int>(i)*41,212,40);
    quickVoice.setBounds(14,225,210,18);
    tryOnlineButton.setBounds(10,247,104,22);
    useVoiceButton.setBounds(122,247,106,22);
    keyButton.setBounds(10,276,218,16);
    presetBox.setBounds(juce::Rectangle<float>(presetBoxX, std::round(row.getCentreY() - 12.0f), presetBoxW, 24.0f).toNearestInt());
    diceButton.setBounds(juce::Rectangle<float>(presetBoxX - 27.0f, std::round(row.getCentreY() - 12.0f), 24.0f, 24.0f).toNearestInt());
    fish.setHome({ juce::roundToInt(row.getX() + logoWordInset), juce::roundToInt(row.getCentreY()) - fish.getHeight() / 2 });

    const auto tools = client(modeWindow);
    for (size_t m = 0; m < modeButtons.size(); ++m)
        modeButtons[m]->setBounds(juce::Rectangle<float>(tools.getX() + 2.0f, tools.getY() + 2.0f + m * 49.0f, tools.getWidth() - 4.0f, 46.0f).toNearestInt());

    seal.setBounds(client(sealWindow).withTrimmedBottom(18.0f).reduced(3.0f).toNearestInt());
    scope.setBounds(client(scopeWindow).withTrimmedBottom(18.0f).reduced(2.0f).toNearestInt());
    {
        // METRO.EXE: BPM glass and the metronome on top, beat lights, five settings rows.
        if (! metroWindow.isPoppedOut())
        metroWindow.setBounds(juce::Rectangle<float>(static_cast<float>(processor.getParameters().state.getProperty("metroX", metroDefault.getX())),
                                                     static_cast<float>(processor.getParameters().state.getProperty("metroY", metroDefault.getY())),
                                                     metroDefault.getWidth(), metroDefault.getHeight()).toNearestInt()
                                  .constrainedWithin(getLocalBounds()));
        const auto m = metroWindow.clientArea();
        bpmField.setBounds(juce::Rectangle<float>(m.getX(), m.getY() + 2.0f, 106.0f, 26.0f).toNearestInt());
        metronomeButton.setBounds(juce::Rectangle<float>(m.getRight() - 30.0f, m.getY(), 30.0f, 30.0f).toNearestInt());
        const auto settingsRow = [&](int i) { return juce::Rectangle<float>(m.getX() + 58.0f, m.getY() + 42.0f + i * 19.0f, m.getWidth() - 58.0f, 16.0f); };
        metronomeVolume.setBounds(settingsRow(0).withTrimmedRight(26.0f).toNearestInt());
        beatsBox.setBounds(settingsRow(1).withWidth(56.0f).toNearestInt());
        accentBox.setBounds(settingsRow(2).withX(m.getX()).withRight(m.getRight()).toNearestInt());
        soundBox.setBounds(settingsRow(3).toNearestInt());
        countInBox.setBounds(settingsRow(4).toNearestInt());
    }
    const auto out = client(outputWindow);
    meter.setBounds(juce::Rectangle<float>(out.getX() + 2.0f, out.getY() + 4.0f, 236.0f, 40.0f).toNearestInt());
    knobs[8]->setBounds(juce::Rectangle<float>(out.getRight() - 136.0f, out.getY(), 136.0f, out.getHeight()).toNearestInt());

    const auto mix = client(mixerWindow);
    for (int i = 0; i < 8; ++i)
    {
        const bool big = i < 4;
        // Four big knobs, the divider, then VOWEL, DETUNE, TIDE and GLIDE.
        // The small ones end above the KYRAX box, so hovering its switches never shows a knob's tooltip.
        const float smallH = juce::jmin(mix.getHeight(), nitroBoxArea().getY() - 2.0f - mix.getY());
        const auto cell = big ? juce::Rectangle<float>(mix.getX() + 4.0f + i * 124.0f, mix.getY(), 124.0f, mix.getHeight())
                              : juce::Rectangle<float>(mix.getX() + 504.0f + (i - 4) * 58.0f, mix.getY(), 58.0f, smallH);
        knobs[static_cast<size_t>(i)]->setBounds(cell.toNearestInt());
    }
    {
        // Standalone transport: [switch | BPM]  [REC | 00:00]; the BPM opens METRO.EXE.
        const auto t = transportArea();
        const float h = t.getHeight(), bpmW = 46.0f;
        barMetronome.setBounds(t.withWidth(h).toNearestInt());
        barTempo.setBounds(t.withX(t.getX() + h + 4.0f).withWidth(bpmW).toNearestInt());
        recordControl.setBounds(t.withTrimmedLeft(h + 4.0f + bpmW + 8.0f).toNearestInt());
    }
    {
        // The KYRAX box: YAP, ARP and NITRO, metronome-size switches with their names, 63 apart.
        const auto box = nitroBoxArea();
        const float s = 19.0f, y = box.getY() + 8.0f;
        yapButton.setBounds(juce::Rectangle<float>(box.getX() + 9.0f, y, s, s).toNearestInt());
        arpButton.setBounds(juce::Rectangle<float>(box.getX() + 9.0f + 63.0f, y, s, s).toNearestInt());
        nitroButton.setBounds(juce::Rectangle<float>(box.getX() + 9.0f + 126.0f, y, s, s).toNearestInt());
        if (nitroWindow.isVisible()) layoutNitro();
        if (arpWindow.isVisible()) layoutArp();
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
    blueScreen.setBounds(getLocalBounds());
}

void PhoqerView::rebuildChrome(float scale)
{
    const int height = baseHeight();
    chromeScale = scale;
    chrome = juce::Image(juce::Image::RGB, juce::roundToInt(width * scale), juce::roundToInt(height * scale), true);
    juce::Graphics g(chrome);
    g.addTransform(juce::AffineTransform::scale(scale));
    const auto& pal = paletteFor(character);
    const juce::String voice = voiceNames[character];

    // Pure Win98 shell: exposed space between and around child windows is system face-grey.
    // Do not wrap the editor in another frame and do not let the old night-sky desktop become a
    // black outer layer. Dark colour now belongs only to the real display and artwork clients.
    g.fillAll(win98::face);

    // Header window: the PHOQER wordmark; the preset box and voice tabs sit on the right.
    const auto head = window98(g, headWindow, "PHOQER.EXE - " + voice, pal, true, headTitleHeight);
    sunken(g,{464,60,252,26},juce::Colour(14,8,24));
    juce::ignoreUnused(head);     // the wordmark is drawn live in paint(), so it can slide aside

    window98(g, modeWindow, "MODE.EXE", pal, false);
    const auto sealClient = window98(g, sealWindow, voice + ".BMP", pal, false);
    sunken(g, sealClient.withTrimmedBottom(18.0f), juce::Colours::black);
    const auto scopeClient = window98(g, scopeWindow, "SCOPE.EXE", pal, false);
    sunken(g, scopeClient.withTrimmedBottom(18.0f), juce::Colours::black);
    window98(g, outputWindow, "LEVEL.EXE", pal, false);
    const auto mix = window98(g, mixerWindow, "MIXER.EXE", pal, false);
    g.setColour(win98::shadow);
    g.fillRect(mix.getX() + 501.0f, mix.getY() + 14.0f, 1.0f, mix.getHeight() - 28.0f);
    g.setColour(win98::light);
    g.fillRect(mix.getX() + 502.0f, mix.getY() + 14.0f, 1.0f, mix.getHeight() - 28.0f);
    if (character == 3)
    {
        // The KYRAX group box and the switches' names: KYRAX only, nothing there on the other voices.
        const auto box = nitroBoxArea();
        g.setColour(win98::shadow);
        g.drawRect(box.withTrimmedRight(1.0f).withTrimmedBottom(1.0f), 1.0f);
        g.setColour(win98::light);
        g.drawRect(box.withTrimmedLeft(1.0f).withTrimmedTop(1.0f), 1.0f);
        g.setColour(win98::shadow);
        g.drawRect(box.withTrimmedRight(1.0f).withTrimmedBottom(1.0f), 1.0f);
        const juce::String caption = "KYRAX";
        const auto font = pixelFont(8.0f, true);
        const auto label = juce::Rectangle<float>(box.getX() + 8.0f, box.getY() - 5.0f, juce::GlyphArrangement::getStringWidth(font, caption) + 6.0f, 10.0f);
        g.setColour(win98::face);
        g.fillRect(label);
        const auto ink = juce::Colours::black;
        drawText(g, caption, label, font, ink, juce::Justification::centred);
        const auto names = pixelFont(10.0f, true);
        const float s = 19.0f, y = box.getY() + 8.0f;
        const char* const switchNames[] { "YAP", "ARP", "NITRO" };
        for (int i = 0; i < 3; ++i)
            drawText(g, switchNames[i], { box.getX() + 9.0f + static_cast<float>(i) * 63.0f + s + 5.0f, y, 60.0f, s }, names, ink);
    }
    if (isStandalone())
    {
        // A Win98 group box round the transport: an etched frame with its caption set into the top line.
        const auto box = transportArea().expanded(10.0f, 8.0f);
        g.setColour(win98::shadow);
        g.drawRect(box.withTrimmedRight(1.0f).withTrimmedBottom(1.0f), 1.0f);
        g.setColour(win98::light);
        g.drawRect(box.withTrimmedLeft(1.0f).withTrimmedTop(1.0f), 1.0f);
        g.setColour(win98::shadow);
        g.drawRect(box.withTrimmedRight(1.0f).withTrimmedBottom(1.0f), 1.0f);
        const juce::String caption = "METRONOME + REC";
        const auto font = pixelFont(8.0f, true);
        const auto label = juce::Rectangle<float>(box.getX() + 8.0f, box.getY() - 5.0f,
                                                  juce::GlyphArrangement::getStringWidth(font, caption) + 6.0f, 10.0f);
        g.setColour(win98::face);
        g.fillRect(label);
        drawText(g, caption, label, font, juce::Colours::black, juce::Justification::centred);
    }
    if (showKeys)
    {
        window98(g, keysWindow, "KEYS.EXE", pal, false);
        sunken(g, piano.getBounds().toFloat().expanded(2.0f), win98::dark);
    }
}

void PhoqerView::paint(juce::Graphics& g)
{
    // The static chrome is cached at the physical pixel scale it is drawn at (zoom included).
    const float physical = juce::jmax(1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (! chrome.isValid() || ! juce::approximatelyEqual(physical, chromeScale)) rebuildChrome(physical);
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(chrome, getLocalBounds().toFloat());
    g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
    g.drawImage(logoWord, logoBounds().translated(std::round(logoSlide), 0.0f));
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    // Live status bars.
    const auto sealClient = windowClient(sealWindow), scopeClient = windowClient(scopeWindow);
    // Under the portrait: the note being sung and its velocity; blank while the seal is quiet.
    const bool singing = processor.isSinging() && processor.getLastNote() >= 0;
    statusBar(g, sealClient.withTrimmedTop(sealClient.getHeight() - 15.0f),
              { singing ? noteName(processor.getLastNote()) : juce::String(),
                singing ? "VEL " + juce::String(processor.getLastVelocity()) : juce::String() });
    const auto notice = updateNotice();     // an update takes the field that otherwise only says TRIG NOTE
    statusBar(g, scopeClient.withTrimmedTop(scopeClient.getHeight() - 15.0f),
              { juce::String(juce::roundToInt(processor.getTelemetry().getSampleRate() / 1000.0f)) + " kHz",
                juce::String(juce::roundToInt(scope.capturedMilliseconds())) + " ms", notice.isNotEmpty() ? notice : "TRIG NOTE" },
              notice.isNotEmpty() ? valueInkFor(character) : juce::Colours::black);
    if (showKeys && isStandalone())
    {
        const auto bar = keysBar();
        const auto octave = "Z/X OCTAVE  A=" + noteName(qwertyBase), velocity = "C/V VELOCITY " + juce::String(qwertyVelocity);
        statusBar(g, bar.withWidth(bar.getWidth() * 0.5f), { octave });     // the left half as before; the right half splits for MONO
        statusBar(g, bar.withTrimmedLeft(bar.getWidth() * 0.5f), { velocity, juce::String("M  ") + (shownMono ? "MONO" : "POLY") });
    }
}

void PhoqerView::timerCallback()
{
    if (! isShowing()) return;     // minimised or hidden: nothing to draw; it all catches up when shown again
    tick(juce::Time::getMillisecondCounterHiRes() * 0.001);
}

void PhoqerView::tick(double now)
{
    updateTick();
    if (monoOn() != shownMono)     // a preset, undo or the host changed MONO: the KEYS.EXE bar follows
    {
        shownMono = monoOn();
        if (showKeys && isStandalone()) repaint(windowClient(keysWindow).toNearestInt());
    }
    if (arpWindow.isVisible())     // the step sounding now, lit pink, follows the ARP
    {
        const int lane = processor.getPlayingArpLane();
        auto lanes = processor.saveArpLanes();     // a song or preset loaded while the window is open
        if (lane != shownArpLane || lanes != shownArpLanes) { shownArpLane = lane; shownArpLanes = std::move(lanes); arpSteps.repaint(); }
    }
    if(character!=processor.getVoices().getVisualVoice())applyCharacter(processor.getVoices().getVisualVoice());
    refreshVoiceControls();
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
    auto& f = ex.face;
    switch (mode)
    {
        case 1:     // HONK: a small round "o", cheeks puffed, eyes wide
            f.mouthRound = 0.6f;
            f.jawWidth = 0.7f;
            f.mouthOpen = juce::jmin(1.0f, f.mouthOpen * 2.0f);
            f.throatTension = juce::jmax(f.throatTension, 0.3f + 0.7f * f.mouthOpen);
            f.eyeOpen = juce::jmin(1.0f, f.eyeOpen + 0.3f);
            f.eyeSquint = 0.0f;
            break;
        case 2:     // BARK: a snapping jaw, kept inside the muzzle, and a squint
            f.mouthRound = 0.0f;
            f.jawWidth = 0.6f;
            f.mouthOpen = juce::jmin(1.0f, f.mouthOpen * 1.2f);
            f.eyeSquint = juce::jmax(f.eyeSquint, 0.5f * f.mouthOpen);
            break;
        case 3:     // WAIL: head tipped back, eyes half shut, a tall singing mouth
            f.headLift = juce::jmax(f.headLift, 0.25f + 0.75f * f.mouthOpen);
            f.eyeOpen = 0.0f;
            f.eyeSquint = 0.45f * juce::jmin(1.0f, f.mouthOpen * 3.0f);
            f.mouthRound = 0.7f;
            f.jawWidth = 0.3f;
            break;
        case 4:     // MURMUR: mouth shut, the hum puffs the cheeks, sleepy eyes
            f.throatTension = juce::jmax(f.throatTension, f.mouthOpen);
            f.mouthOpen = 0.0f;
            f.eyeOpen = 0.15f;
            f.eyeSquint = 0.3f;
            break;
        default:    // CALL: as the engine reports
            f.jawWidth = juce::jmin(f.jawWidth, 0.7f);
            break;
    }

    // The fish held over the face: wide eyes, looking up, mouth open and panting for it.
    if (fish.isHeld() && seal.getBounds().contains(fish.getBounds().getCentre()))
    {
        const float pant = 0.5f + 0.5f * std::sin(static_cast<float>(now) * juce::MathConstants<float>::twoPi * 4.0f);
        f.eyeOpen = 1.0f;
        f.eyeSquint = 0.0f;
        f.headLift = juce::jmax(f.headLift, 0.7f);
        f.mouthOpen = juce::jmax(f.mouthOpen, 0.35f + 0.25f * pant);
        f.mouthRound = 0.5f;
        f.jawWidth = 0.5f;
        f.throatTension = juce::jmax(f.throatTension, 0.4f);
        ex.grin = 0.0f;
    }
    // Fed: it munches the fish (three chomps), gulps it down (a lump travels down the throat), then
    // sits back with a closed smile and one slow, contented blink. The eyes stay ^ ^ throughout.
    else if (now >= happyFrom && now < happyUntil)
    {
        const float age = static_cast<float>(now - happyFrom), span = static_cast<float>(happyUntil - happyFrom);
        const float in = juce::jmin(1.0f, age / 0.15f), out = juce::jmin(1.0f, (span - age) / 0.3f);
        ex.happyEyes = in * out;
        ex.grin = 0.0f;
        f.eyeSquint = 0.0f;
        f.eyeOpen = 0.6f;
        if (age < 1.2f)                                         // munch, munch, munch
        {
            f.mouthOpen = 0.6f * juce::jmax(0.0f, std::sin(age * juce::MathConstants<float>::twoPi * 2.5f));
            f.mouthRound = 0.3f;
            f.jawWidth = 0.4f;
            f.throatTension = juce::jmax(f.throatTension, 0.45f);
        }
        else
        {
            f.mouthOpen = 0.0f;
            if (age < 1.9f) ex.gulp = (age - 1.2f) / 0.7f;          // down it goes
            ex.smile = juce::jmin(1.0f, (age - 1.2f) / 0.4f) * out;
            if (age > 2.3f && age < 2.48f)                          // a slow, contented blink
            {
                f.eyeOpen = 0.2f;
                f.eyeSquint = 0.55f;
            }
        }
    }
    // Every other frame, or at once when the mouth moves: a 100 ms honk must not fall between redraws.
    const bool happy = now >= happyFrom && now < happyUntil;     // the happy sequence needs every frame
    if ((frame % 2) == 0 || happy || std::abs(grin - previousGrin) > 0.05f || std::abs(f.mouthOpen - shownMouth) > 0.08f)
    {
        seal.update(portrait, character, ex);
        shownMouth = f.mouthOpen;
    }
    scope.refresh();
    meter.refresh();

    const auto& recorder = processor.getRecorder();
    const auto countIn = recorder.countInRemaining();
    const double beatSamples = 60.0 / juce::jmax(20.0f, processor.getParameters().getRawParameterValue("standaloneTempo")->load())
                             * juce::jmax(1.0, recorder.sampleRate());
    recordControl.setState(recorder.isRecording(), recorder.isRecording() ? recorder.seconds() : 0.0,
                           countIn > 0 ? static_cast<int>(std::ceil(static_cast<double>(countIn) / beatSamples)) : 0);
    const bool clicking = metronomeButton.getToggleState() || countIn > 0;
    if (clicking || metronomeWasOn) { metronomeButton.repaint(); barMetronome.repaint(); }     // the arms swing
    metronomeWasOn = clicking;
    const int light = clicking ? static_cast<int>(std::floor(processor.getMetronomeBeats())) % processor.getMetronomeBeatsPerBar() : -1;
    barTempo.setText(juce::String(juce::roundToInt(processor.getParameters().getRawParameterValue("standaloneTempo")->load())));
    if (light != shownBeatLight && isStandalone())
    {
        shownBeatLight = light;
        const auto m = metroWindow.clientArea();
        metroWindow.repaint(juce::Rectangle<float>(m.getX() + 28.0f, m.getY() + 31.0f, 76.0f, 4.0f).toNearestInt());
    }
    if (const auto text = presets.displayName(); text != shownPreset)
    {
        shownPreset = text;
        presetBox.repaint();
    }

    // Keep the computer keyboard live: take focus whenever the window is active, and let go of
    // held notes when it is not (key-up events would never arrive).
    if (auto* peer = getPeer(); isStandalone() && peer != nullptr && peer->isFocused())
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

    if (crashAt > 0.0 && now >= crashAt)
    {
        crashAt = 0.0;
        releaseQwertyNotes();
        fish.setVisible(false);
        blueScreen.toFront(false);
        blueScreen.setVisible(true);
    }

    // The secret: the wordmark eases aside, and the fish shows once it has room (and once a
    // fish that was just eaten has been replaced).
    if (fishOut && ! fish.isVisible() && fishBackAt > 0.0 && now >= fishBackAt && logoSlide > fish.getWidth() * 0.6f)
    {
        fishBackAt = 0.0;
        fish.setVisible(true);
    }
    if (const float goal = fishOut ? fishSlide() : 0.0f; ! juce::approximatelyEqual(logoSlide, goal))
    {
        logoSlide = std::abs(goal - logoSlide) < 0.5f ? goal : logoSlide + (goal - logoSlide) * 0.27f;
        fish.setVisible(logoSlide > fish.getWidth() * 0.6f && now >= fishBackAt);
        diceButton.setVisible(logoSlide < 0.5f);     // the fish's wordmark slides over the dice's place
        repaint(logoBounds().getUnion(logoBounds().translated(fishSlide(), 0.0f)).expanded(2.0f).toNearestInt());
    }
    ++frame;
}

// =================================================================================== EDITOR
PhoqerEditor::PhoqerEditor(PhoqerAudioProcessor& owner) : AudioProcessorEditor(owner), view(owner, laf)
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
    const float fits = juce::jmin(static_cast<float>(area.getWidth()) / PhoqerView::width,
                                  static_cast<float>(area.getHeight()) / static_cast<float>(view.baseHeight()));
    setZoom(juce::jmax(minZoom(), juce::jmin(UiSettings::get().zoom(), fits)));
    remembersZoom = true;
}

PhoqerEditor::~PhoqerEditor()
{
    setLookAndFeel(nullptr);
}

juce::Rectangle<int> PhoqerEditor::screenArea() const
{
    const auto& displays = juce::Desktop::getInstance().getDisplays();
    const auto* display = isShowing() ? displays.getDisplayForRect(getScreenBounds()) : displays.getPrimaryDisplay();
    // Leave room for a host's window frame; the standalone window has none.
    return display != nullptr ? display->userArea.reduced(16, 40) : juce::Rectangle<int>(1280, 720);
}

void PhoqerEditor::updateLimits()
{
    const int h = view.baseHeight();
    setResizeLimits(juce::roundToInt(PhoqerView::width * minZoom()), juce::roundToInt(static_cast<float>(h) * minZoom()),
                    juce::roundToInt(PhoqerView::width * maxZoom()), juce::roundToInt(static_cast<float>(h) * maxZoom()));
    if (auto* c = getConstrainer()) c->setFixedAspectRatio(static_cast<double>(PhoqerView::width) / h);
}

void PhoqerEditor::setZoom(float z)
{
    z = juce::jlimit(minZoom(), maxZoom(), z);
    setSize(juce::roundToInt(PhoqerView::width * z), juce::roundToInt(static_cast<float>(view.baseHeight()) * z));
    zoom = z;
}

void PhoqerEditor::fitToScreen()
{
    const auto area = screenArea();
    const float z = juce::jmin(static_cast<float>(area.getWidth()) / PhoqerView::width,
                               static_cast<float>(area.getHeight()) / static_cast<float>(view.baseHeight()));
    setZoom(std::floor(z * 100.0f) / 100.0f);
}

void PhoqerEditor::resized()
{
    const float z = static_cast<float>(getWidth()) / PhoqerView::width;    // the height follows the fixed aspect ratio
    if (z <= 0.0f) return;
    zoom = z;
    view.setTransform(juce::AffineTransform::scale(z));
    if (remembersZoom) UiSettings::get().setZoom(z);
}
}
