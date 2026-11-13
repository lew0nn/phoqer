// Panels that open over the window: WAIL BEND and the draggable floating windows (METRO.EXE, VOICES.EXE).

#include "Views.h"

#include "../Core/FishSprite.h"

#include "Style.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace phoqer::ui
{
WailBendPanel98::WailBendPanel98(juce::RangedAudioParameter& p, juce::RangedAudioParameter& hold)
    : attachment(p, slider), lockAttachment(hold, lock)
{
    popOut.moveArea = [this](juce::Point<int> at) { setTopLeftPosition(at); };
    lock.setComponentID("wailHold");
    lock.setTooltip("LOCK: WAIL RISES AND STAYS AT THE TOP");
    lock.setWantsKeyboardFocus(false);
    addAndMakeVisible(lock);
    setComponentID("wailBend");
    slider.setComponentID("wailBendSlider");
    value.setComponentID("wailBendValue");
    slider.setTooltip("WAIL BEND: HOW FAR WAIL SWEEPS UP AND DOWN");
    value.setTooltip("DOUBLE-CLICK TO TYPE 0-24");
    slider.setWantsKeyboardFocus(false);
    slider.onValueChange = [this] { show(static_cast<float>(slider.getValue())); };
    value.setEditable(false, true, false);
    value.setJustificationType(juce::Justification::centred);
    value.setBorderSize(juce::BorderSize<int>(0));
    value.onEditorShow = [this]
    {
        if (onBeginEdit) onBeginEdit();
        if (auto* editor = value.getCurrentTextEditor())
        {
            editor->setInputRestrictions(4, "0123456789.,");
            editor->setJustification(juce::Justification::centred);
            editor->setText(juce::String(slider.getValue(), 1), false);
            editor->selectAll();
        }
    };
    value.onTextChange = [this]
    {
        const auto text = value.getText().replace(",", ".").retainCharacters("0123456789.");
        if (text.containsAnyOf("0123456789"))
            slider.setValue(juce::jlimit(0.0, 24.0, std::round(text.getDoubleValue() * 10.0) / 10.0), juce::sendNotificationSync);
        show(static_cast<float>(slider.getValue()));
    };
    addAndMakeVisible(slider);
    addAndMakeVisible(value);
    setCharacter(0);
    show(static_cast<float>(slider.getValue()));
}

WailBendPanel98::~WailBendPanel98() { juce::Desktop::getInstance().removeGlobalMouseListener(&outside); }

void WailBendPanel98::show(float v) { value.setText(juce::String(v, 1) + " ST", juce::dontSendNotification); }

void WailBendPanel98::setCharacter(int c)
{
    character = c;
    const auto& pal = paletteFor(character);
    const auto ink = character == 3 ? approvedHoloColour(paletteFor(2).accent.darker(.6f)) : pal.accent.darker(.6f);     // as the knobs' values
    value.setFont(pixelFont(10.0f, true));
    for (const auto id : { juce::Label::textColourId, juce::Label::textWhenEditingColourId }) value.setColour(id, ink);
    for (const auto id : { juce::Label::backgroundColourId, juce::Label::backgroundWhenEditingColourId }) value.setColour(id, juce::Colours::white);
    repaint();
}

void WailBendPanel98::open(juce::Rectangle<int> beside, juce::Component* openedBy)
{
    opener = openedBy;
    popOut.dock();
    setBounds(beside.getRight() + 1, beside.getY() + 5, 185, 48);
    setVisible(true);
    toFront(false);
    juce::Desktop::getInstance().addGlobalMouseListener(&outside);
}

void WailBendPanel98::close()
{
    juce::Desktop::getInstance().removeGlobalMouseListener(&outside);
    value.hideEditor(false);
    popOut.dock();
    setVisible(false);
}

void WailBendPanel98::Outside::mouseDown(const juce::MouseEvent& e)
{
    auto* c = e.eventComponent;
    if (c == &panel || panel.isParentOf(c) || c == panel.opener || (panel.opener != nullptr && panel.opener->isParentOf(c))) return;
    panel.close();
}

void WailBendPanel98::resized()
{
    slider.setBounds(6, 24, getWidth() - 12, 14);
    value.setBounds(98, 7, 59, 13);
    lock.setBounds(162, 5, 17, 17);
}

void WailBendPanel98::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    button98(g, r, false);
    drawText(g, "WAIL BEND", { 8.0f, 5.0f, 88.0f, 16.0f }, pixelFont(10.0f, true), juce::Colours::black, juce::Justification::centredLeft);
    sunken(g, value.getBounds().toFloat().expanded(2.0f), juce::Colours::white);
    const auto track = slider.getBounds().toFloat().reduced(4.0f, 0.0f);     // ticks at 0, 6, 12, 18 and 24 semitones
    g.setColour(juce::Colours::black);
    for (int st = 0; st <= 24; st += 6)
        g.fillRect(juce::Rectangle<float>(std::round(track.getX() + track.getWidth() * static_cast<float>(st) / 24.0f), track.getBottom() + 1.0f, 1.0f, 4.0f));
}

void WailBendPanel98::Padlock::paintButton(juce::Graphics& g, bool, bool down)
{
    const auto r = getLocalBounds().toFloat();
    const bool in = getToggleState() || down;
    if (in) sunken(g, r, juce::Colour(180, 180, 180)); else button98(g, r, false);
    static const char* art[] { "  ###  ", " #   # ", " #   # ", "#######", "### ###", "### ###", "#######" };
    const float x0 = std::round(r.getCentreX() - 3.5f), y0 = std::round(r.getCentreY() - 3.5f);
    g.setColour(juce::Colours::black);
    for (int y = 0; y < 7; ++y)
        for (int x = 0; x < 7; ++x)
            if (art[y][x] == '#') g.fillRect(x0 + x, y0 + y, 1.0f, 1.0f);
}

FloatingWindow98::FloatingWindow98(const juce::String& titleText) : title(titleText)
{
    close.onClick = [this] { setVisible(false); if (onClose) onClose(); };
    addAndMakeVisible(close);
    popOut.moveArea = [this](juce::Point<int> p) { setTopLeftPosition(p); };
    popOut.onChange = [this](bool out) { if (! out && onMoved) onMoved(); };
}

PopOut98::PopOut98(juce::Component& c, std::function<juce::Rectangle<int>()> a) : owner(c), area(std::move(a)) {}

PopOut98::~PopOut98()
{
    stopTimer();
    if (holder != nullptr)
    {
        holder->removeChildComponent(&owner);
        holder->setLookAndFeel(nullptr);
        holder = nullptr;
    }
}

void PopOut98::mouseDown(const juce::MouseEvent& e)
{
    grab = e.getEventRelativeTo(&owner).position - area().getPosition().toFloat();
    if (holder != nullptr) { holder->toFront(owner.getWantsKeyboardFocus()); following = true; startTimerHz(60); }     // out already: the timer follows the mouse
}

void PopOut98::mouseDrag(const juce::MouseEvent& e)
{
    if (holder != nullptr) return;
    auto* parent = owner.getParentComponent();
    if (parent == nullptr) return;
    const auto mouse = e.getScreenPosition();
    const auto box = area();
    const auto topLeft = (parent->getLocalPoint(nullptr, mouse).toFloat() - grab).roundToInt();
    if (parent->getLocalBounds().contains(box.withPosition(topLeft))) { if (moveArea) moveArea(topLeft); }
    else popOut(mouse);     // past the editor's edge: out it comes
}

void PopOut98::popOut(juce::Point<int> mouseOnScreen)
{
    home = owner.getParentComponent();
    if (home == nullptr || ! home->isShowing()) return;
    scale = static_cast<float>(home->getScreenBounds().getWidth()) / static_cast<float>(juce::jmax(1, home->getWidth()));
    const auto box = area();
    auto h = std::make_unique<juce::Component>();
    h->setLookAndFeel(&home->getLookAndFeel());
    h->setMouseClickGrabsKeyboardFocus(owner.getWantsKeyboardFocus());     // false: a click does not activate it (MA_NOACTIVATE)
    h->setSize(juce::roundToInt(box.getWidth() * scale), juce::roundToInt(box.getHeight() * scale));
    h->setTopLeftPosition(mouseOnScreen - (grab * scale).roundToInt());
    holder = std::move(h);          // set before the move, so the owner can tell it is going out
    if (onChange) onChange(true);
    holder->addAndMakeVisible(owner);
    owner.setTopLeftPosition(-box.getX(), -box.getY());
    owner.setTransform(juce::AffineTransform::scale(scale));
    holder->addToDesktop(0);        // a plain window: no taskbar button, no OS frame (it draws its own)
    holder->setAlwaysOnTop(true);
    onTop = true;
    holder->setVisible(true);
    following = true;
    startTimerHz(60);
}

void PopOut98::dock()
{
    if (holder == nullptr) return;
    stopTimer();
    following = false;
    const auto at = home != nullptr ? home->getLocalPoint(nullptr, holder->getScreenPosition()) : juce::Point<int>();
    auto out = std::move(holder);   // cleared before the move, so the owner can tell it is back
    owner.setTransform({});
    out->removeChildComponent(&owner);
    if (home != nullptr)
    {
        home->addChildComponent(owner);
        owner.setTopLeftPosition(0, 0);
        const auto box = area(), inside = home->getLocalBounds();
        const auto clamped = juce::Point<int>(juce::jlimit(0, juce::jmax(0, inside.getWidth() - box.getWidth()), at.x),
                                              juce::jlimit(0, juce::jmax(0, inside.getHeight() - box.getHeight()), at.y));
        if (moveArea) moveArea(clamped);
    }
    out->setLookAndFeel(nullptr);
    out = nullptr;
    if (onChange) onChange(false);
}

void PopOut98::timerCallback()
{
    if (holder == nullptr) { stopTimer(); return; }
    if (following)
    {
        holder->setTopLeftPosition(juce::Desktop::getMousePosition() - (grab * scale).roundToInt());
        if (! juce::ModifierKeys::getCurrentModifiersRealtime().isAnyMouseButtonDown())
        {
            following = false;
            if (home != nullptr && home->getScreenBounds().contains(holder->getScreenBounds())) { dock(); return; }
        }
    }
    // The keys stay with PHOQER: if the window got the focus anyway, it goes back, unless a value is being typed.
    if (! owner.getWantsKeyboardFocus() && home != nullptr && holder->hasKeyboardFocus(true)
        && dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent()) == nullptr)
        home->grabKeyboardFocus();
    // On top of PHOQER (and the DAW) while that app is in front; not over other apps.
    if (const bool front = juce::Process::isForegroundProcess(); front != onTop)
    {
        onTop = front;
        holder->setAlwaysOnTop(front);
    }
}

FloatingWindow98::~FloatingWindow98() = default;

void FloatingWindow98::visibilityChanged()
{
    if (! isVisible() && popOut.isOut()) popOut.dock();     // closed while out: it opens inside the editor next time
}

juce::Rectangle<float> FloatingWindow98::clientArea() const { return windowClient(getLocalBounds().toFloat()); }

void FloatingWindow98::paint(juce::Graphics& g)
{
    const auto bounds=getLocalBounds().toFloat();
    const auto client = paintFrame ? paintFrame(g,bounds) : window98(g,bounds,title,paletteFor(character),true);
    if (paintContent) paintContent(g, client);
}

void FloatingWindow98::resized()
{
    const auto c = clientArea();
    close.setBounds(juce::Rectangle<float>(c.getRight() - 14.0f, c.getY() - defaultTitleHeight - 1.0f, 14.0f, 12.0f).toNearestInt());
}

void FloatingWindow98::mouseDown(const juce::MouseEvent& e)
{
    toFront(false);
    dragging = e.position.y < clientArea().getY();
    if (dragging) popOut.mouseDown(e);
}

void FloatingWindow98::mouseDrag(const juce::MouseEvent& e) { if (dragging) popOut.mouseDrag(e); }

void FloatingWindow98::mouseUp(const juce::MouseEvent&)
{
    if (dragging && ! popOut.isOut() && onMoved) onMoved();
    dragging = false;
}
}
