// The Win98 dialog box: title, lines, an optional text field, buttons, and the keyboard-shortcut table.

#include "Views.h"

#include "../Core/FishSprite.h"

#include "Style.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace phoqer::ui
{
// --------------------------------------------------------------------------------------- DIALOG
namespace
{
constexpr float dialogWidth = 360.0f, dialogTitle = 20.0f, lineHeight = 16.0f, fieldHeight = 22.0f, buttonW = 76.0f, buttonH = 24.0f;
constexpr float headingHeight = 26.0f, rowHeight = 21.0f, gapHeight = 8.0f;

float heightOf(const juce::String& line) noexcept
{
    if (line.isEmpty()) return gapHeight;
    if (line.startsWith("#")) return headingHeight;
    if (line.containsChar('|')) return rowHeight;
    return lineHeight;
}

// "[CTRL]+[Z]": each bracketed key a small raised Win98 keycap, anything between them plain text.
void drawKeys(juce::Graphics& g, const juce::String& keys, float x, float y)
{
    const auto capFont = pixelFont(10.0f, true), joinFont = pixelFont(12.0f);
    for (int i = 0; i < keys.length();)
    {
        if (keys[i] == '[')
        {
            const int end = keys.indexOfChar(i, ']');
            if (end < 0) break;
            const auto label = keys.substring(i + 1, end);
            const float w = std::max(18.0f, juce::GlyphArrangement::getStringWidth(capFont, label) + 12.0f);
            const juce::Rectangle<float> cap { x, y + 1.0f, w, rowHeight - 4.0f };
            g.setColour(win98::face);
            g.fillRect(cap);
            bevel(g, cap, true);
            drawText(g, label, cap.translated(0.0f, -1.0f), capFont, juce::Colours::black, juce::Justification::centred);
            x += w + 3.0f;
            i = end + 1;
        }
        else
        {
            const auto join = juce::String::charToString(keys[i]);
            if (join != " ")
            {
                const float w = juce::GlyphArrangement::getStringWidth(joinFont, join) + 4.0f;
                drawText(g, join, { x, y, w, rowHeight - 2.0f }, joinFont, juce::Colours::black, juce::Justification::centred);
                x += w + 2.0f;
            }
            else
                x += 3.0f;
            ++i;
        }
    }
}
}

float Dialog98::linesHeight() const
{
    float h = 0.0f;
    for (const auto& line : spec.lines) h += heightOf(line);
    return h;
}

Dialog98::Dialog98(Spec s) : spec(std::move(s))
{
    setWantsKeyboardFocus(true);
    if (spec.fieldLabel.isNotEmpty())
    {
        field.setFont(pixelFont(12.0f));
        field.setText(spec.fieldText, false);
        field.setColour(juce::TextEditor::backgroundColourId, juce::Colours::white);
        field.setColour(juce::TextEditor::textColourId, juce::Colours::black);
        field.setColour(juce::TextEditor::highlightColourId, paletteFor(spec.character).accent.darker(0.35f));
        field.setColour(juce::TextEditor::highlightedTextColourId, juce::Colours::white);
        field.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        field.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        field.setColour(juce::CaretComponent::caretColourId, juce::Colours::black);
        field.setIndents(4, 4);
        field.onReturnKey = [this] { finish(0); };
        field.onEscapeKey = [this] { if (spec.closable) finish(-1); };
        addAndMakeVisible(field);
    }
    for (int i = 0; i < spec.buttons.size(); ++i)
    {
        auto b = std::make_unique<PushButton98>(spec.buttons[i], i == 0);
        b->onClick = [this, i] { finish(i); };
        addAndMakeVisible(*b);
        buttons.push_back(std::move(b));
    }
    closeButton.onClick = [this] { finish(-1); };
    addChildComponent(closeButton);
    closeButton.setVisible(spec.closable);
    // Out of the editor: the box goes, a clear blocker stays so the editor still waits for the box.
    popOut.moveArea = [this](juce::Point<int> at)
    {
        offset += at.toFloat() - boxBounds().getPosition();
        resized();
        repaint();
    };
    popOut.onChange = [this](bool out)
    {
        if (out) { if (auto* p = getParentComponent()) { p->addAndMakeVisible(blocker); blocker.setBounds(p->getLocalBounds()); } }
        else if (auto* p = blocker.getParentComponent()) p->removeChildComponent(&blocker);
    };
}

void Dialog98::mouseDown(const juce::MouseEvent& e)
{
    const auto box = boxBounds();
    dragging = e.position.y >= box.getY() && e.position.y < box.getY() + 4.0f + dialogTitle && box.contains(e.position);
    if (dragging) popOut.mouseDown(e);
}

void Dialog98::mouseDrag(const juce::MouseEvent& e) { if (dragging) popOut.mouseDrag(e); }

void Dialog98::parentHierarchyChanged()
{
    if (auto* p = getParentComponent())
    {
        if (! popOut.isOut()) setBounds(p->getLocalBounds());     // out, it keeps the editor's size inside its window
        if (spec.fieldLabel.isNotEmpty()) { field.grabKeyboardFocus(); field.selectAll(); }
        else grabKeyboardFocus();
    }
}

juce::Rectangle<float> Dialog98::boxBounds() const
{
    const bool hasField = spec.fieldLabel.isNotEmpty();
    const float body = std::max(spec.icon.isValid() ? static_cast<float>(spec.icon.getHeight()) * 2.0f : 0.0f,
                                linesHeight() + (hasField ? fieldHeight + 10.0f : 0.0f));
    const float h = 4.0f + dialogTitle + 16.0f + body + 18.0f + buttonH + 14.0f;
    return juce::Rectangle<float>(std::max(dialogWidth, spec.width), h).withCentre(getLocalBounds().toFloat().getCentre() + offset).toNearestInt().toFloat();
}

void Dialog98::resized()
{
    const auto box = boxBounds();
    closeButton.setBounds(juce::Rectangle<float>(box.getRight() - 4.0f - 2.0f - 16.0f, box.getY() + 6.0f, 16.0f, 14.0f).toNearestInt());
    const float textX = box.getX() + 16.0f + (spec.icon.isValid() ? static_cast<float>(spec.icon.getWidth()) * 2.0f + 16.0f : 0.0f);
    if (spec.fieldLabel.isNotEmpty())
    {
        const float y = box.getY() + 4.0f + dialogTitle + 16.0f + linesHeight();
        const float labelW = juce::GlyphArrangement::getStringWidth(pixelFont(12.0f), spec.fieldLabel) + 10.0f;
        field.setBounds(juce::Rectangle<float>(textX + labelW + 2.0f, y + 2.0f, box.getRight() - 18.0f - textX - labelW - 4.0f, fieldHeight - 4.0f).toNearestInt());
    }
    const float total = static_cast<float>(buttons.size()) * buttonW + static_cast<float>(buttons.size() - 1) * 8.0f;
    float bx = std::round(box.getCentreX() - total * 0.5f);
    for (auto& b : buttons)
    {
        b->setBounds(juce::Rectangle<float>(bx, box.getBottom() - 14.0f - buttonH, buttonW, buttonH).toNearestInt());
        bx += buttonW + 8.0f;
    }
}

void Dialog98::paint(juce::Graphics& g)
{
    const auto box = boxBounds();
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillRect(box.translated(4.0f, 4.0f));
    g.setColour(win98::face);
    g.fillRect(box);
    bevel(g, box, true);
    const juce::Rectangle<float> bar { box.getX() + 4.0f, box.getY() + 4.0f, box.getWidth() - 8.0f, dialogTitle };
    g.setColour(paletteFor(spec.character).accent.darker(0.35f));
    g.fillRect(bar);
    drawText(g, spec.title, bar.reduced(5.0f, 0.0f), pixelFont(12.0f, true), juce::Colours::white);

    float x = box.getX() + 16.0f, y = bar.getBottom() + 16.0f;
    if (spec.icon.isValid())
    {
        g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
        g.setOpacity(1.0f);
        const float iw = static_cast<float>(spec.icon.getWidth()) * 2.0f, ih = static_cast<float>(spec.icon.getHeight()) * 2.0f;
        g.drawImage(spec.icon, { x, y, iw, ih });
        x += iw + 16.0f;
    }
    const auto ink = paletteFor(spec.character).accent.darker(0.35f);
    const float right = box.getRight() - 16.0f;
    for (int i = 0; i < spec.lines.size(); ++i)
    {
        const auto& line = spec.lines[i];
        if (line.startsWith("#"))                               // a section heading, then an etched rule to the edge
        {
            const auto text = line.substring(1);
            const auto font = pixelFont(12.0f, true);
            const float ty = y + 6.0f, tw = juce::GlyphArrangement::getStringWidth(font, text);
            drawText(g, text, { x, ty, tw + 2.0f, lineHeight }, font, ink);
            const float ry = std::round(ty + lineHeight * 0.5f);
            g.setColour(win98::shadow); g.fillRect(x + tw + 8.0f, ry, right - x - tw - 8.0f, 1.0f);
            g.setColour(win98::light);  g.fillRect(x + tw + 8.0f, ry + 1.0f, right - x - tw - 8.0f, 1.0f);
        }
        else if (line.containsChar('|'))                        // two columns: a name or keys, then what it does
        {
            const auto left = line.upToFirstOccurrenceOf("|", false, false), text = line.fromFirstOccurrenceOf("|", false, false);
            if (left.startsWith("["))
                drawKeys(g, left, x, y);
            else
                drawText(g, left, { x, y + 2.0f, spec.labelWidth - 6.0f, lineHeight }, pixelFont(12.0f, true), ink);
            drawText(g, text, { x + spec.labelWidth, y + 2.0f, right - x - spec.labelWidth, lineHeight }, pixelFont(12.0f), juce::Colours::black);
        }
        else if (line.isNotEmpty())
        {
            const bool quiet = line.startsWith("~");            // "~" marks a secondary (grey) line
            drawText(g, quiet ? line.substring(1) : line, { x, y, right - x, lineHeight }, pixelFont(12.0f, i == 0 && ! quiet && spec.icon.isValid()),
                     quiet ? win98::shadow : juce::Colours::black);
        }
        y += heightOf(line);
    }
    if (spec.fieldLabel.isNotEmpty())
    {
        drawText(g, spec.fieldLabel, { x, y, 100.0f, fieldHeight }, pixelFont(12.0f), juce::Colours::black);
        sunken(g, field.getBounds().toFloat().expanded(2.0f));
    }
}

bool Dialog98::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::returnKey) { finish(0); return true; }
    if (key == juce::KeyPress::escapeKey) { if (spec.closable) finish(-1); return true; }
    return true;                                               // a modal box swallows everything else
}

void Dialog98::finish(int button)
{
    if (finished) return;
    finished = true;
    auto callback = spec.onClose;
    const auto text = field.getText().trim();
    // The owner deletes this dialog in the callback, so run it after this call has returned.
    juce::MessageManager::callAsync([callback, button, text] { if (callback) callback(button, text); });
}
}
