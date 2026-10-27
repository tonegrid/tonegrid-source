#pragma once
#include "Shared/UI/PopupStyle.h"
#include <ToneGridApplicationAssets.h>
namespace tonegrid::libraryui {
class BreadcrumbButton final : public juce::TextButton {
public:
    explicit BreadcrumbButton(const juce::String& title) : TextButton(title) {}
    void paintButton(juce::Graphics& g, bool hover, bool) override {
        g.setColour(hover ? juce::Colour(0xff2098f4) : ui::ink());
        g.setFont(ui::font(14));
        g.drawText(getButtonText(), getLocalBounds().withTrimmedRight(16), juce::Justification::centredLeft, true);
        g.setColour(ui::metadataInk());
        g.drawLine(float(getWidth()-10),11,float(getWidth()-6),15,1);
        g.drawLine(float(getWidth()-6),15,float(getWidth()-10),19,1);
    }
};
class OnlineCaptureButton final : public juce::Button {
public:
    OnlineCaptureButton() : Button("Get more captures on TONE3000") {}
    void paintButton(juce::Graphics& g, bool hover, bool down) override {
        using namespace juce;
        ui::paintControl(g,getLocalBounds().toFloat(),false,hover,down,true);
        if (getWidth() >= 280) {
            g.setColour(ui::ink());g.setFont(ui::font(14));
            g.drawText(tonegrid::l10n::translate("Get more captures on"),12,0,142,getHeight(),Justification::centredLeft);
        }
        const auto image = ImageCache::getFromMemory(ToneGridApplicationAssets::Tone3000_png,ToneGridApplicationAssets::Tone3000_pngSize);
        if (image.isValid()) g.drawImage(image,float(getWidth()>=280?getWidth()-134:(getWidth()-120)/2),(getHeight()-24.f)/2,120.f,24.f,0,80,200,40);
    }
};
class LibraryBrowserShell : public popup::View {
protected:
    static void paintBrowserPanels(juce::Graphics& g, juce::Rectangle<int> left,
                                   juce::Rectangle<int> center, juce::Rectangle<int> right) {
        for (auto panel : {left, center, right}) {
            g.setColour(juce::Colour(0xff0b141a));g.fillRoundedRectangle(panel.toFloat(),10);
            g.setColour(ui::outline().withAlpha(.4f));g.drawRoundedRectangle(panel.toFloat().reduced(.5f),10,1);
        }
        g.setColour(ui::metadataInk());g.setFont(ui::font(12));
        g.drawText(tonegrid::l10n::translate("LIBRARY"),left.reduced(18).withHeight(20),juce::Justification::centredLeft);
    }
};
}
