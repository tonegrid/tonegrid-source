#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace tonegrid::ui
{
#if JUCE_MAC
// AppKit permits a window to grow wider than the display. Keep the entire
// native frame in the current work area so the scaled board stays reachable.
class WorkAreaConstrainer final : public juce::ComponentBoundsConstrainer
{
public:
    void checkBounds(juce::Rectangle<int>& bounds,const juce::Rectangle<int>& previous,
                     const juce::Rectangle<int>& limits,bool top,bool left,bool bottom,bool right) override
    {
        ComponentBoundsConstrainer::checkBounds(bounds,previous,limits,top,left,bottom,right);
        if(const auto* display=juce::Desktop::getInstance().getDisplays().getDisplayForRect(bounds))
            bounds=bounds.constrainedWithin(display->userArea);
    }
};
#endif
// Keep the existing layout at normal sizes. Below its design minimum, render
// live components with a uniform transform, rather than clipping or a bitmap.
class ScalableBoard final : public juce::Component
{
public:
    explicit ScalableBoard(std::unique_ptr<juce::Component> content) : board(std::move(content))
    {
        addAndMakeVisible(*board);
        setSize(board->getWidth(), board->getHeight());
    }
    juce::Component& content() const { return *board; }
    void resized() override
    {
        if (getWidth() <= 0 || getHeight() <= 0) return;
        const float scale = juce::jmin(1.f, getWidth() / 1360.f, getHeight() / 820.f);
        board->setTransform(juce::AffineTransform::scale(scale));
        board->setBounds(0, 0, int(std::floor(getWidth() / scale)), int(std::floor(getHeight() / scale)));
    }
    static juce::Rectangle<int> initialBounds(juce::Rectangle<int> workArea)
    {
        // Work areas are already in logical coordinates, including display DPI
        // scaling and excluding the taskbar / macOS menu and Dock.
        const auto available = workArea.reduced(20);
        return available.withSizeKeepingCentre(juce::jmin(1660, available.getWidth()),
                                                juce::jmin(940, available.getHeight()));
    }
private:
    std::unique_ptr<juce::Component> board;
};
}
