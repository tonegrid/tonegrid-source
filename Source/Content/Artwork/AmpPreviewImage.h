#pragma once
#include <juce_graphics/juce_graphics.h>
#include <cmath>

namespace tonegrid::ampui
{
// GUI-only cache. Always derive a new display size from the original pixels,
// never from the previous thumbnail. No file access or audio-thread work.
class PreviewImage final
{
public:
    void set(juce::Image image)
    {
        if(source==image)return;
        source=std::move(image);display={};
    }
    bool valid()const{return source.isValid();}
    const juce::Image& forPixels(int width,int height)
    {
        if(!valid()){display={};return display;}
        width=juce::jlimit(1,source.getWidth(),width);
        height=juce::jlimit(1,source.getHeight(),height);
        if(display.isValid()&&display.getWidth()==width&&display.getHeight()==height)return display;
        display=source;
        // A single large bilinear reduction undersamples grille cloth, lettering
        // and fine edges. Halving first filters those details instead of aliasing.
        while(display.getWidth()>=width*2&&display.getHeight()>=height*2)
            display=display.rescaled(juce::jmax(width,display.getWidth()/2),
                                    juce::jmax(height,display.getHeight()/2),
                                    juce::Graphics::highResamplingQuality);
        display=display.rescaled(width,height,juce::Graphics::highResamplingQuality);
        return display;
    }
    void draw(juce::Graphics& g,juce::Rectangle<float> bounds,int placement=juce::RectanglePlacement::centred)
    {
        if(!valid()||bounds.isEmpty())return;
        const auto fitted=juce::RectanglePlacement(placement)
            .appliedTo(source.getBounds().toFloat(),bounds);
        // Includes native display DPI and any parent/snapshot transform. Moving
        // between standard and Retina/HiDPI screens rebuilds only this cache.
        const auto scale=g.getInternalContext().getPhysicalPixelScaleFactor();
        const auto& image=forPixels(juce::jmax(1,int(std::ceil(fitted.getWidth()*scale))),
                                   juce::jmax(1,int(std::ceil(fitted.getHeight()*scale))));
        juce::Graphics::ScopedSaveState saved(g);
        g.setOpacity(1.f);g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(image,fitted,juce::RectanglePlacement::stretchToFit);
    }
private:
    juce::Image source,display;
};
}
