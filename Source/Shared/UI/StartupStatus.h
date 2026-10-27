#pragma once
#include "Shared/UI/PopupStyle.h"

namespace tonegrid::ui
{
// Startup feedback belongs to the UI; neither the loader nor audio callback
// touches this component. Stages describe real work, not estimated percentages.
class StartupStatus final : public juce::Component
{
public:
    StartupStatus(){setComponentID("startup-status");setTitle("Preparing your sound");setAlwaysOnTop(true);}
    void stage(const juce::String& value){detail=value;setDescription(value);repaint();}
    void advance(double seconds){angle=float(std::fmod(seconds*3.5,juce::MathConstants<double>::twoPi));repaint();}
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black.withAlpha(.62f));
        const auto box=getLocalBounds().toFloat().withSizeKeepingCentre(juce::jmin(560.f,float(getWidth()-32)),228.f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff1b262e),box.getTopLeft(),juce::Colour(0xff0b1116),box.getBottomRight(),false));
        g.fillRoundedRectangle(box,18);g.setColour(popup::outline());g.drawRoundedRectangle(box,18,1.2f);
        const auto spinner=juce::Rectangle<float>(0,0,30,30).withCentre({box.getCentreX(),box.getY()+44});
        g.setColour(ui::tabBlue().withAlpha(.18f));g.drawEllipse(spinner,3);
        juce::Path arc;arc.addCentredArc(spinner.getCentreX(),spinner.getCentreY(),15,15,0,angle,angle+4.3f,true);
        g.setColour(ui::tabBlue());g.strokePath(arc,juce::PathStrokeType(3,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        g.setColour(popup::ink());g.setFont(ui::font(24));g.drawText(tonegrid::l10n::translate("Preparing your sound..."),box.withY(box.getY()+78).withHeight(34),juce::Justification::centred);
        g.setFont(ui::font(17));g.drawFittedText(l10n::translate(detail),box.withY(box.getY()+121).withHeight(44).toNearestInt(),juce::Justification::centred,2);
        g.setColour(popup::mutedInk());g.setFont(ui::font(14));g.drawText(tonegrid::l10n::translate("ToneGrid will be ready to play shortly."),box.withY(box.getY()+169).withHeight(24),juce::Justification::centred);
    }
private:
    juce::String detail="Loading pedals";float angle=0;
};
}
