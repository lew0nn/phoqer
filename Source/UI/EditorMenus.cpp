// The menu bar's menus and what they do: presets, undo, reset and randomise, keys, zoom, panic, copy and paste.
#include "EditorShared.h"

namespace phoqer::ui
{
// ------------------------------------------------------------------------------------------ MENUS

void PhoqerView::addPresetItems(juce::PopupMenu& menu)
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

juce::PopupMenu PhoqerView::buildMenu(int index)
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
            addItem(m, "REVERT TO PRESET", id++, presets.currentIndex() >= 0 && presets.isDirty(), false, {},
                    [this] { loadPreset(presets.currentIndex()); });
            m.addSeparator();
            addItem(m, "COPY SETTINGS", id++, true, false, {}, [this] { copySettings(); });
            addItem(m, "PASTE SETTINGS", id++, true, false, {}, [this] { pasteSettings(); });
            m.addSeparator();
            addItem(m, monoOn() ? "PLAY: MONO" : "PLAY: POLY", id++, true, false, isStandalone() ? "M" : "", [this] { toggleMono(); });
            m.addSeparator();
            addItem(m, "PANIC (ALL NOTES OFF)", id++, true, false, {}, [this] { panic(); });
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
            addItem(m, "KEYBOARD SHORTCUTS...", id++, true, false, {}, [this] { showShortcuts(); });
            addItem(m, "CONTROLS GUIDE...", id++, true, false, {}, [this] { showGuide(); });
            m.addSeparator();
            addItem(m, "VISIT GITHUB...", id++, true, false, {},
                    [] { juce::URL("https://github.com/lew0nn/phoqer").launchInDefaultBrowser(); });
            addItem(m, "LICENCES...", id++, true, false, {}, [this] { showLicences(); });
            m.addSeparator();
            addItem(m, "CHECK FOR UPDATES...", id++, ! checkingUpdates, false, {}, [this] { checkForUpdates(true); });
            addItem(m, "CHECK EVERY DAY", id++, true, UiSettings::get().checkUpdatesDaily(), {},
                    [] { UiSettings::get().setCheckUpdatesDaily(! UiSettings::get().checkUpdatesDaily()); });
            m.addSeparator();
            addItem(m, "ABOUT PHOQER...", id++, true, false, {}, [this] { showAbout(); });
            break;

        default: break;
    }
    return m;
}

void PhoqerView::showMenu(int index)
{
    if (dialog != nullptr || ! juce::isPositiveAndBelow(index, MenuBar98::itemCount)) return;
    auto menu = buildMenu(index);
    const auto title = menuBar.itemBounds(index);
    showPopup(menu, title.translated(static_cast<float>(menuBar.getX()), static_cast<float>(menuBar.getY())), index);
}

// Drops a menu down from an area of this view. The target component makes the menu scale with the zoom.
void PhoqerView::showPopup(juce::PopupMenu& menu, juce::Rectangle<float> target, int menuIndex)
{
    releaseQwertyNotes();
    menu.setLookAndFeel(&lookAndFeel);
    menuBar.setOpenIndex(menuIndex);
    const auto screenArea = localAreaToGlobal(target.toNearestInt());
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea(screenArea)
                                                 .withMinimumWidth(140),
                       [safe = juce::Component::SafePointer<PhoqerView>(this)](int)
                       {
                           if (safe != nullptr) safe->menuBar.setOpenIndex(-1);
                       });
}

// WAIL BEND: click WAIL again while it is on, or right-click it: the panel opens beside the button (or closes).
void PhoqerView::showWailBend()
{
    if (wailBendPanel.isVisible()) { wailBendPanel.close(); return; }
    releaseQwertyNotes();
    wailBendPanel.open(modeButtons[3]->getBounds(), modeButtons[3].get());
}

// ---------------------------------------------------------------------------------------- ACTIONS
void PhoqerView::resetKnobs()
{
    history.performAsOne([this]
    {
        for (const auto* id : { "boom", "air", "bark", "space", "vowel", "detune", "tide", "glide", "output" })
        {
            auto& p = parameterFor(processor.getParameters(), id);
            p.beginChangeGesture();
            p.setValueNotifyingHost(p.getDefaultValue());
            p.endChangeGesture();
        }
    });
}

void PhoqerView::randomizeKnobs()
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

void PhoqerView::loadPreset(int index)
{
    history.performAsOne([this, index] { presets.load(index); });
}

void PhoqerView::stepPreset(int delta)
{
    presets.refresh();
    const int count = static_cast<int>(presets.all().size());
    if (count == 0) return;
    const int from = presets.currentIndex() < 0 ? (delta > 0 ? -1 : 0) : presets.currentIndex();
    loadPreset(((from + delta) % count + count) % count);
}

void PhoqerView::toggleKeys()
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

bool PhoqerView::monoOn() const
{
    return processor.getParameters().getRawParameterValue("mono")->load() >= 0.5f;
}

void PhoqerView::toggleMono()
{
    auto& p = parameterFor(processor.getParameters(), "mono");
    p.beginChangeGesture();
    p.setValueNotifyingHost(monoOn() ? 0.0f : 1.0f);
    p.endChangeGesture();
}

void PhoqerView::panic()
{
    releaseQwertyNotes();
    processor.getKeyboardState().allNotesOff(0);
    processor.panic();
}

// A sound as one line of text, to paste into a message and back: "PHOQER character=0 behavior=2 boom=0.750 ..."
void PhoqerView::copySettings()
{
    juce::String text = "PHOQER";
    for (const auto& id : PresetLibrary::parameterIds())
        if (auto* v = processor.getParameters().getRawParameterValue(id))
            text << " " << id << "=" << juce::String(v->load(), 3);
    juce::SystemClipboard::copyTextToClipboard(text);
    showMessage("COPY SETTINGS", { "THE SOUND IS ON THE CLIPBOARD.", "~PASTE IT IN A MESSAGE, OR BACK HERE" });
}

void PhoqerView::pasteSettings()
{
    const auto tokens = juce::StringArray::fromTokens(juce::SystemClipboard::getTextFromClipboard().trim(), " \t\r\n", {});
    const auto ids = PresetLibrary::parameterIds();
    std::vector<std::pair<juce::String, float>> values;
    if (tokens.size() > 1 && tokens[0] == "PHOQER")
        for (int i = 1; i < tokens.size(); ++i)
        {
            const auto key = tokens[i].upToFirstOccurrenceOf("=", false, false);
            if (ids.contains(key) && tokens[i].containsChar('='))
                values.emplace_back(key, tokens[i].fromFirstOccurrenceOf("=", false, false).getFloatValue());
        }
    if (values.empty())
    {
        showMessage("PASTE SETTINGS", { "NO PHOQER SOUND ON THE CLIPBOARD.", "~COPY ONE WITH EDIT > COPY SETTINGS" });
        return;
    }
    history.performAsOne([this, &values]
    {
        for (const auto& [id, value] : values)
        {
            auto& p = parameterFor(processor.getParameters(), id.toRawUTF8());
            p.beginChangeGesture();
            p.setValueNotifyingHost(p.convertTo0to1(value));
            p.endChangeGesture();
        }
    });
}
}
