#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <ToneGridBranding.h>
namespace tonegrid
{
class Logo final:public juce::Component
{
public:
    Logo()
    {
        drawable=juce::Drawable::createFromImageData(ToneGridBranding::ToneGrid_logo_transparent_svg,ToneGridBranding::ToneGrid_logo_transparent_svgSize);
        if(drawable){drawable->replaceColour(juce::Colours::black,juce::Colours::white);artBounds=drawable->getOutlineAsPath().getBounds();}
        mark=part("logo-mark");wordmark=part("wordmark");
        setInterceptsMouseClicks(false,false);setComponentID("tonegrid-svg-logo");
    }
    void paint(juce::Graphics& g) override
    {
        const auto bounds=getLocalBounds().toFloat().reduced(4);const auto draw=[&](juce::Drawable* d,juce::Rectangle<float> target){if(d)d->draw(g,1.f,juce::RectanglePlacement(juce::RectanglePlacement::centred).getTransformToFit(d->getOutlineAsPath().getBounds(),target));};
        draw(mark.get(),bounds.withWidth(32));draw(wordmark.get(),bounds.withTrimmedLeft(42));
    }
    bool valid() const noexcept {return drawable&&!artBounds.isEmpty();}
private:
    static std::unique_ptr<juce::Drawable> part(const juce::String& id)
    {
        auto svg=juce::XmlDocument::parse(juce::String::fromUTF8(ToneGridBranding::ToneGrid_logo_transparent_svg,ToneGridBranding::ToneGrid_logo_transparent_svgSize));if(!svg)return {};
        auto* group=svg->getChildByName("g");if(!group)return {};for(int i=group->getNumChildElements()-1;i>=0;--i)if(group->getChildElement(i)->getStringAttribute("id")!=id)group->removeChildElement(group->getChildElement(i),true);
        auto result=juce::Drawable::createFromSVG(*svg);if(result)result->replaceColour(juce::Colours::black,juce::Colours::white);return result;
    }
    std::unique_ptr<juce::Drawable> drawable,mark,wordmark;juce::Rectangle<float> artBounds;
};
}
