#pragma once
#include "TunerDisplay.h"
#include "Shared/UI/PopupWindow.h"
#include <ToneGridBranding.h>

namespace tonegrid::tuning
{
class TunerButton final : public juce::Button
{
public:
    explicit TunerButton(bool isClose) : Button(isClose ? "Close tuner" : "Mute"), closing(isClose)
    { setTitle(getName()); }
private:
    void paintButton(juce::Graphics& g, bool hover, bool down) override
    {
        using namespace juce;
        const auto bounds = getLocalBounds().toFloat().reduced(1);
        const auto ink = Colour(0xffe0e3e2).withAlpha(down ? .6f : hover ? 1.f : .85f);
        g.setColour(ink);
        if (closing)
        {
            const auto c = bounds.getCentre();
            g.drawLine(c.x - 4, c.y - 4, c.x + 4, c.y + 4, 1.2f);
            g.drawLine(c.x - 4, c.y + 4, c.x + 4, c.y - 4, 1.2f);
            if (hasKeyboardFocus(true)) g.drawRoundedRectangle(bounds, 5, 1);
            return;
        }
        g.setColour(Colour(getToggleState() ? 0xff293235 : hover ? 0xff1c2426 : 0xff101719));
        g.fillRoundedRectangle(bounds, 9);
        g.setColour(Colour(hasKeyboardFocus(true) ? 0xffc1ccca : 0xff465154));
        g.drawRoundedRectangle(bounds, 9, 1);
        g.setColour(ink);
        const float y = bounds.getCentreY(), x = 20;
        Path speaker;
        speaker.startNewSubPath(x - 5, y - 3);
        speaker.lineTo(x - 1, y - 3); speaker.lineTo(x + 4, y - 7);
        speaker.lineTo(x + 4, y + 7); speaker.lineTo(x - 1, y + 3);
        speaker.lineTo(x - 5, y + 3); speaker.closeSubPath();
        g.fillPath(speaker);
        if (getToggleState())
        {
            g.drawLine(x + 8, y - 3, x + 14, y + 3, 1.2f);
            g.drawLine(x + 8, y + 3, x + 14, y - 3, 1.2f);
        }
        else
        {
            Path wave; wave.startNewSubPath(x + 8, y - 5);
            wave.quadraticTo(x + 14, y, x + 8, y + 5);
            g.strokePath(wave, PathStrokeType(1.2f));
        }
        Font font(FontOptions("Segoe UI", 11.f, Font::plain));
        font.setExtraKerningFactor(.12f); g.setFont(font);
        g.drawText(tonegrid::l10n::translate("MUTE"), 44, 0, getWidth() - 51, getHeight(), Justification::centred);
    }
    bool closing;
};

class Controls final : public juce::Component, private juce::Timer
{
public:
    Controls(Engine& e, AudioTap& audio) : engine(e), tap(audio)
    {
        mark = logoPart("logo-mark");
        title.setText("TUNER", juce::dontSendNotification); title.setComponentID("popup-title");
        title.setColour(juce::Label::textColourId, juce::Colour(0xffe5e8e7)); title.setBorderSize({});
        title.setInterceptsMouseClicks(false, false); addAndMakeVisible(title);
        mute.setClickingTogglesState(true); mute.setComponentID("tuner-mute");
        tonegrid::l10n::tooltip(mute,"Mute the output while tuning"); addAndMakeVisible(mute);
        mute.onClick = [this] { tap.mute(mute.getToggleState()); };
        closeButton.setComponentID("tuner-close"); tonegrid::l10n::tooltip(closeButton,"Close tuner (Esc)");
        closeButton.onClick = [this] { if (auto* window = findParentComponentOfClass<juce::DialogWindow>()) window->closeButtonPressed(); };
        addAndMakeVisible(closeButton);
        constrainer.setMinimumOnscreenAmounts(0x10000, 48, 48, 48);
        setSize(560, 440);
    }
    ~Controls() override { stopTimer(); }
    void open()
    {
        engine.enable(true); tap.mute(true); mute.setToggleState(true, juce::dontSendNotification);
        display.reset(); lastTick = juce::Time::getMillisecondCounterHiRes(); startTimerHz(60); repaint();
    }
    void close() { stopTimer(); engine.enable(false); tap.mute(false); display.reset(); }
    void updateReading(const Reading& reading, double elapsedSeconds)
    { display.advance(reading, elapsedSeconds); repaint(); }

    void paint(juce::Graphics& g) override
    {
        using namespace juce;
        Graphics::ScopedSaveState saved(g);
        g.addTransform(AffineTransform::scale(float(getWidth()) / 560.f, float(getHeight()) / 440.f));
        const juce::Rectangle<float> frame(2, 2, 556, 436);
        g.setGradientFill(ColourGradient(Colour(0xff1b2022), 230, 165, Colour(0xff080c0e), 560, 440, true));
        g.fillRoundedRectangle(frame, 20);
        g.setColour(Colour(0xff384347)); g.drawRoundedRectangle(frame, 20, 1.5f);
        g.setColour(Colour(0xff515b5c).withAlpha(.12f));
        g.drawRoundedRectangle(frame.reduced(2), 18, 1);
        drawLogo(g, mark.get(), {32, 31, 34, 34});
        auto smallText = [&](const String& text, float size, int x, int y, int w, Justification align)
        {
            Font font(FontOptions("Segoe UI", size, Font::plain)); font.setExtraKerningFactor(.14f);
            g.setFont(font); g.setColour(Colour(0xffd3d8d7)); g.drawText(text, x, y, w, 22, align);
        };
        smallText("CHROMATIC", 10, 384, 37, 125, Justification::centredRight);

        const double position = std::clamp(display.indicatorCents, -50.0, 50.0) * 6.0 / 50.0;
        for (int i = -6; i <= 6; ++i)
        {
            const float x = 280.f + float(i) * 38.f, y = 181.f;
            const float radius = 3.f + float(6 - std::abs(i)) * .45f;
            const float light = display.valid ? float(std::exp(-.5 * std::pow((i - position) / .60, 2))) : 0.f;
            const auto active = display.inTune() && i == 0 ? Colour(0xff4fefb4) : Colour(0xffe2e8e6);
            if (light > .01f)
            {
                g.setGradientFill(ColourGradient(active.withAlpha(.18f * light), x, y,
                                                active.withAlpha(0.f), x + 22, y, true));
                g.fillEllipse(x - 22, y - 22, 44, 44);
            }
            const auto idle = Colour(0xffa1aaa9).withAlpha(.22f + float(6 - std::abs(i)) * .07f);
            g.setColour(idle.interpolatedWith(active, light));
            const float r = radius + light * 2.5f;
            g.fillEllipse(x - r, y - r, r * 2, r * 2);
        }
        smallText("FLAT", 11, 42, 214, 100, Justification::centredLeft);
        smallText("SHARP", 11, 418, 214, 100, Justification::centredRight);
        String note = String::charToString(0x2014), details = "Play one string";
        if (display.valid)
        {
            static const StringArray names{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
            note = names[display.note % 12] + String(display.note / 12 - 1);
            const auto cents = std::abs(display.textCents) < .05 ? 0.0 : display.textCents;
            details = (cents >= 0 ? "+" : "") + String(cents, 1) + " cents";
        }
        g.setColour(Colour(0xffe5e8e7)); g.setFont(FontOptions("Segoe UI", 64.f, Font::plain));
        g.drawText(note, 40, 246, 480, 82, Justification::centred);
        g.setColour(Colour(0xffbdc5c3)); g.setFont(FontOptions("Segoe UI", 14.f, Font::plain));
        g.drawText(details, 40, 326, 480, 24, Justification::centred);
        smallText("A = 440 Hz", 11, 32, 386, 150, Justification::centredLeft);
    }
    void resized() override
    {
        const float sx = float(getWidth()) / 560.f, sy = float(getHeight()) / 440.f;
        const auto scale = juce::AffineTransform::scale(sx, sy);
        title.setFont(juce::FontOptions("Segoe UI", 22.f, juce::Font::plain));
        title.setBounds(juce::Rectangle<float>(81, 31, 280, 34).transformedBy(scale).toNearestInt());
        mute.setBounds(juce::Rectangle<float>(402, 375, 124, 40).transformedBy(scale).toNearestInt());
        closeButton.setBounds(getWidth()-37,12,24,24);
    }
    void mouseDown(const juce::MouseEvent& event) override
    {
        dragging = event.position.y < float(getHeight()) * .22f;
        if (dragging) dragger.startDraggingComponent(getTopLevelComponent(), event);
    }
    void mouseDrag(const juce::MouseEvent& event) override
    { if (dragging) dragger.dragComponent(getTopLevelComponent(), event, &constrainer); }

private:
    static std::unique_ptr<juce::Drawable> logoPart(const juce::String& id)
    {
        auto svg = juce::XmlDocument::parse(juce::String::fromUTF8(ToneGridBranding::ToneGrid_logo_transparent_svg,
                                                               ToneGridBranding::ToneGrid_logo_transparent_svgSize));
        if (!svg) return {};
        auto* group = svg->getChildByName("g");
        if (!group) return {};
        for (int i = group->getNumChildElements() - 1; i >= 0; --i)
        {
            auto* child = group->getChildElement(i);
            if (child->getStringAttribute("id") != id) group->removeChildElement(child, true);
        }
        auto result = juce::Drawable::createFromSVG(*svg);
        if (result) result->replaceColour(juce::Colours::black, juce::Colours::white);
        return result;
    }
    static void drawLogo(juce::Graphics& g, juce::Drawable* logo, juce::Rectangle<float> bounds)
    {
        if (logo) logo->draw(g, .9f, juce::RectanglePlacement(juce::RectanglePlacement::centred)
                           .getTransformToFit(logo->getOutlineAsPath().getBounds(), bounds));
    }
    void timerCallback() override
    {
        const auto now = juce::Time::getMillisecondCounterHiRes();
        if (isShowing())
        {
            updateReading(engine.reading(), (now - lastTick) / 1000.0);
            mute.setToggleState(tap.isMuted(), juce::dontSendNotification); repaint();
        }
        lastTick = now;
    }
    Engine& engine;
    AudioTap& tap;
    Display display;
    std::unique_ptr<juce::Drawable> mark;
    juce::Label title;
    TunerButton mute{false}, closeButton{true};
    juce::ComponentDragger dragger;
    juce::ComponentBoundsConstrainer constrainer;
    double lastTick = 0;
    bool dragging = false;
};

// JUCE retains modal lifetime, Escape handling, DPI scaling and keyboard focus.
class Window final : public popup::DismissibleDialog
{
public:
    Window(Controls& controls, juce::Component& owner)
        : popup::DismissibleDialog("Tuner",owner)
    {
        setUsingNativeTitleBar(false); setTitleBarHeight(0); setTitleBarButtonsRequired(0, false); setOpaque(false);
        setResizable(true, false); setDropShadowEnabled(true);
        setContentNonOwned(&controls, true);
        const auto area=owner.getParentMonitorArea();setResizeLimits(360,280,32768,32768);
        setBounds(popup::fittedBounds(area,{560,440},owner.getScreenBounds().getCentre()));
        border=std::make_unique<popup::ResizeBorder>(*this,getConstrainer());addAndMakeVisible(*border);resized();
    }
    void resized()override{DialogWindow::resized();if(border)border->setBounds(getLocalBounds().reduced(2));}
    juce::BorderSize<int> getBorderThickness() const override { return {}; }
    void closeButtonPressed() override { exitModalState(0);setVisible(false); }
private:
    std::unique_ptr<popup::ResizeBorder> border;
};
}
