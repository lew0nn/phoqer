#pragma once

#include <JuceHeader.h>

namespace phoqer::ui
{
struct Layout final
{
    static constexpr int width = 684;
    static constexpr int height = 520;
    static constexpr int outerMargin = 18;
    static constexpr int sectionGap = 10;
    static constexpr int headerHeight = 46;
    static constexpr int controlBandHeight = 162;
    static constexpr int sidebarWidth = 51;
    static constexpr int monitorWidth = 332;
    static constexpr int meterHeight = 54;
    static constexpr int tideColumnWidth = 92;
    static constexpr int knobDiameter = 108;
    static constexpr int labelHeight = 20;
    static constexpr int labelGap = 10;

    static juce::Rectangle<int> editor() noexcept { return { 0, 0, width, height }; }
    static juce::Rectangle<int> inner() noexcept { return editor().reduced(outerMargin); }

    static juce::Rectangle<int> header() noexcept
    {
        auto area = inner();
        return area.removeFromTop(headerHeight);
    }

    static juce::Rectangle<int> mainArea() noexcept
    {
        auto area = inner();
        area.removeFromTop(headerHeight + sectionGap);
        area.removeFromBottom(controlBandHeight + sectionGap);
        return area;
    }

    static juce::Rectangle<int> controlBand() noexcept
    {
        auto area = inner();
        return area.removeFromBottom(controlBandHeight);
    }

    static juce::Rectangle<int> sidebar() noexcept
    {
        auto area = mainArea();
        return area.removeFromLeft(sidebarWidth);
    }

    static juce::Rectangle<int> modeButton(int index) noexcept
    {
        auto area = sidebar();
        constexpr int buttonHeight = 45;
        constexpr int gap = 4;
        constexpr int stackHeight = buttonHeight * 5 + gap * 4;
        const auto top = area.getY() + (area.getHeight() - stackHeight) / 2;
        return { area.getX(), top + index * (buttonHeight + gap),
                 area.getWidth(), buttonHeight };
    }

    static juce::Rectangle<int> visualArea() noexcept
    {
        auto area = mainArea();
        area.removeFromLeft(sidebarWidth + sectionGap);
        return area;
    }

    static juce::Rectangle<int> monitor() noexcept
    {
        auto area = visualArea();
        return area.removeFromRight(monitorWidth);
    }

    static juce::Rectangle<int> seal() noexcept
    {
        auto area = visualArea();
        area.removeFromRight(monitorWidth + sectionGap);
        return area;
    }

    static juce::Rectangle<int> meter() noexcept
    {
        auto area = monitor();
        return area.removeFromBottom(meterHeight);
    }

    static juce::Rectangle<int> waveform() noexcept
    {
        auto area = monitor();
        area.removeFromBottom(meterHeight + 10);
        return area;
    }

    static juce::Rectangle<int> menu() noexcept
    {
        auto area = header();
        return area.removeFromRight(40).withSizeKeepingCentre(38, 34);
    }

    static juce::Rectangle<int> knobArea() noexcept
    {
        auto area = controlBand();
        area.removeFromRight(tideColumnWidth + sectionGap);
        return area;
    }

    static juce::Rectangle<int> knobCell(int index) noexcept
    {
        auto area = knobArea();
        const auto cellWidth = area.getWidth() / 4;
        return { area.getX() + index * cellWidth, area.getY(), cellWidth, area.getHeight() };
    }

    static juce::Rectangle<int> knob(int index) noexcept
    {
        auto cell = knobCell(index);
        const auto label = cell.removeFromBottom(labelHeight);
        return { cell.getCentreX() - knobDiameter / 2,
                 label.getY() - labelGap - knobDiameter,
                 knobDiameter, knobDiameter };
    }

    static juce::Rectangle<int> knobLabel(int index) noexcept
    {
        auto cell = knobCell(index);
        return cell.removeFromBottom(labelHeight);
    }

    static juce::Rectangle<int> tideCell() noexcept
    {
        auto area = controlBand();
        return area.removeFromRight(tideColumnWidth);
    }

    static juce::Rectangle<int> tide() noexcept
    {
        auto cell = tideCell();
        const auto label = cell.removeFromBottom(labelHeight);
        constexpr int tideWidth = 76;
        constexpr int tideHeight = 144;
        return { cell.getCentreX() - tideWidth / 2,
                 label.getY() - labelGap - tideHeight,
                 tideWidth, tideHeight };
    }

    static juce::Rectangle<int> tideLabel() noexcept
    {
        auto cell = tideCell();
        return cell.removeFromBottom(labelHeight);
    }
};
}
