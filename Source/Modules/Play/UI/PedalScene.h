#pragma once
#include "Modules/Play/UI/PedalBoardLayout.h"

namespace tonegrid::assets
{
// Landmarks measured in the approved 1942 x 809 PNG. The full source image
// remains visible; its two light bands follow the actual row bases on resize.
inline constexpr std::array<int,4> studioSourceY{0,419,694,809};
inline std::array<int,4> studioDestinationY(juce::Rectangle<int> area)
{
    const auto cards=legacyBoardBounds(area);
    return {area.getY(),cards[0].getBottom()+3,cards[7].getBottom()+3,area.getBottom()};
}
inline void paintStudioBackground(juce::Graphics& g,juce::Rectangle<int> area)
{
    if(area.isEmpty())return;
    juce::Graphics::ScopedSaveState saved(g);g.reduceClipRegion(area);
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    const auto image=background();const auto y=studioDestinationY(area);
    for(size_t band=0;band<3;++band)
    {
        const auto strip=image.getClippedImage({0,studioSourceY[band],image.getWidth(),studioSourceY[band+1]-studioSourceY[band]});
        g.drawImage(strip,juce::Rectangle<float>((float)area.getX(),(float)y[band],(float)area.getWidth(),(float)(y[band+1]-y[band])));
    }
}
inline void paintPedalContactShadows(juce::Graphics& g,juce::Rectangle<int> area)
{
    if(area.isEmpty())return;
    juce::Graphics::ScopedSaveState saved(g);g.reduceClipRegion(area);
    for(const auto& card:legacyBoardBounds(area))
    {
        // The existing component clip defines the visible base of the enclosure.
        // Paint in its parent so the soft shadow can extend below that base.
        juce::Path contact;contact.addRoundedRectangle((float)card.getX()+card.getWidth()*.05f,
            (float)card.getBottom()-1.f,card.getWidth()*.9f,3.f,1.5f);
        const auto radius=juce::jlimit(4,7,juce::roundToInt(card.getHeight()*.016f));
        juce::DropShadow(juce::Colours::black.withAlpha(.65f),radius,{0,2}).drawForPath(g,contact);
        g.setColour(juce::Colours::black.withAlpha(.38f));g.fillPath(contact);
    }
}
inline void paintLegacyPedalScene(juce::Graphics& g,juce::Rectangle<int> area)
{paintStudioBackground(g,area);paintPedalContactShadows(g,area);}
inline void paintPedalScene(juce::Graphics& g,juce::Rectangle<int> area)
{g.setColour(juce::Colour(0xff0c151a));g.fillRect(area);}
}
