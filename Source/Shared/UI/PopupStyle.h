#pragma once
#include "AmpChassis.h"
#include <vector>

namespace tonegrid::popup
{
inline juce::Colour ink() { return ui::ink(); }
inline juce::Colour mutedInk() { return ui::secondaryInk(); }
inline juce::Colour outline() { return ui::outline(); }
inline juce::Colour selected() { return ui::selected(); }
inline juce::Colour field() { return ui::panel(); }

// This look is attached only to popup components; the pedalboard keeps its look.
class Look final : public juce::LookAndFeel_V4
{
public:
    Look()
    {
        using namespace juce;
        setColour(Label::textColourId, ink());
        setColour(TextButton::buttonColourId, field()); setColour(TextButton::buttonOnColourId, selected());
        setColour(TextButton::textColourOffId, ink()); setColour(TextButton::textColourOnId, ink());
        setColour(ToggleButton::textColourId, ink()); setColour(ToggleButton::tickColourId, ink());
        setColour(ToggleButton::tickDisabledColourId, mutedInk());
        setColour(ComboBox::backgroundColourId, field()); setColour(ComboBox::textColourId, ink());
        setColour(ComboBox::outlineColourId, outline()); setColour(ComboBox::arrowColourId, ink());
        setColour(ComboBox::focusedOutlineColourId, mutedInk());
        setColour(PopupMenu::backgroundColourId, field()); setColour(PopupMenu::textColourId, ink());
        setColour(PopupMenu::highlightedBackgroundColourId, selected()); setColour(PopupMenu::highlightedTextColourId, ink());
        setColour(PopupMenu::headerTextColourId, mutedInk());
        setColour(TextEditor::backgroundColourId, field()); setColour(TextEditor::textColourId, ink());
        setColour(TextEditor::outlineColourId, outline()); setColour(TextEditor::focusedOutlineColourId, mutedInk());
        setColour(TextEditor::highlightColourId, selected()); setColour(TextEditor::highlightedTextColourId, ink());
        setColour(CaretComponent::caretColourId, ink());
        setColour(Slider::textBoxTextColourId, ink()); setColour(Slider::textBoxBackgroundColourId, field());
        setColour(Slider::textBoxOutlineColourId, outline()); setColour(Slider::textBoxHighlightColourId, selected());
        setColour(Slider::thumbColourId, mutedInk()); setColour(Slider::trackColourId, selected());
        setColour(Slider::backgroundColourId, field());
        setColour(ScrollBar::thumbColourId, juce::Colour(0xff879391));
        setColour(ScrollBar::backgroundColourId, Colours::transparentBlack);
        setColour(ListBox::backgroundColourId, field()); setColour(ListBox::outlineColourId, outline());
        setColour(TooltipWindow::backgroundColourId, field()); setColour(TooltipWindow::textColourId, ink());
        setColour(TooltipWindow::outlineColourId, outline());
        setColour(ResizableWindow::backgroundColourId, field());
    }
    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float position,
                          float start, float end, juce::Slider& slider) override
    { metal.drawRotarySlider(g, x, y, w, h, position, start, end, slider); }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&, bool hover, bool down) override
    {
        ui::paintControl(g,button.getLocalBounds().toFloat(),button.getToggleState(),hover,down,button.isEnabled(),button.hasKeyboardFocus(true));
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) override
    {
        using namespace juce;
        g.setColour(ink().withAlpha(button.isEnabled() ? 1.f : ui::disabledOpacity));
        g.setFont(ui::font(ui::controlSize));
        auto bounds = button.getLocalBounds().reduced(10, 2);
        const auto id = button.getComponentID();
        if (id == "nam-library-refresh" || id == "nam-library-open-folder")
        {
            const float x = float(bounds.getX()) + 9, y = float(bounds.getCentreY());
            Path icon;
            if (id == "nam-library-open-folder")
            {
                icon.startNewSubPath(x - 9, y + 6); icon.lineTo(x - 9, y - 7);
                icon.lineTo(x - 3, y - 7); icon.lineTo(x, y - 4);
                icon.lineTo(x + 9, y - 4); icon.lineTo(x + 9, y + 6); icon.closeSubPath();
                icon.startNewSubPath(x - 9, y - 2); icon.lineTo(x + 9, y - 2);
            }
            else
            {
                icon.addCentredArc(x, y, 7, 7, 0, -.8f, 4.6f, true);
                icon.startNewSubPath(x - 7, y - 4); icon.lineTo(x - 7, y + 1); icon.lineTo(x - 2, y + 1);
            }
            g.strokePath(icon, PathStrokeType(1.5f, PathStrokeType::curved, PathStrokeType::rounded));
            bounds.removeFromLeft(28);
        }
        g.drawFittedText(button.getButtonText(), bounds, Justification::centred, 1);
    }
    void drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& combo) override
    {
        ui::paintControl(g,{0.f,0.f,float(width),float(height)},false,combo.isMouseOver(),false,combo.isEnabled(),combo.hasKeyboardFocus(true));
        ui::paintChevron(g,combo.getLocalBounds().toFloat(),ink().withMultipliedAlpha(combo.isEnabled()?1.f:ui::disabledOpacity));
    }

    juce::Font getComboBoxFont(juce::ComboBox&) override { return ui::font(ui::controlSize); }
    void drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height) override
    {
        g.setColour(field()); g.fillRoundedRectangle(0, 0, float(width), float(height), 6);
        g.setColour(outline()); g.drawRoundedRectangle(.5f, .5f, float(width - 1), float(height - 1), 6, 1);
        g.setColour(ink()); g.setFont(ui::font(ui::controlSize));
        g.drawFittedText(text, 7, 5, width - 14, height - 10, juce::Justification::centredLeft, 12);
    }
private:
    ampui::Look metal;
};

class View : public juce::Component
{
public:
    View() : languageSubscription([this]{languageChanged();resized();repaint();}) { setLookAndFeel(&theme); }
    ~View() override { setLookAndFeel(nullptr); }
    // Optional, body-owned action displayed in the common window header.
    virtual bool popupBranding() const {return true;}
    virtual bool popupLocalised() const {return true;}
    virtual juce::Component* popupHeaderAction() { return nullptr; }
    virtual std::vector<juce::Component*> popupFooterActions() { return {}; }
    virtual int popupHeaderWidth(int) const {return 300;}
    virtual bool popupStackHeader(int) const {return false;}
    virtual int popupHeaderHeight(int) const {return 42;}
    // Fixed panels keep their design width and scroll. Responsive panels opt in
    // to a smaller width and can report their natural height after reflow.
    virtual int popupMinimumWidth() const {return 0;}
    virtual int popupContentHeight(int) const {return 0;}
    virtual float popupCornerRadius() const {return 20.f;}
    virtual float popupBorderWidth() const {return 1.5f;}
    virtual void languageChanged() {}
private:
    Look theme;
    l10n::Subscription languageSubscription;
};

}
