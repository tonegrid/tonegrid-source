#pragma once
#include "Shared/UI/UiStyle.h"

namespace tonegrid::ui
{
// Presentation only: keep RigHost's status text and accessibility value intact.
class StatusLabel final : public juce::Label
{
public:
    void paint(juce::Graphics& g) override
    {
        const auto parts=juce::StringArray::fromTokens(getText(),"|",{});
        auto r=getLocalBounds().toFloat().reduced(4,0);g.setFont(ui::font(statusSize));
        for(int i=0;i<parts.size()&&r.getWidth()>0;++i)
        {
            const auto text=parts[i].trim();
            if(i==0)
            {
                const auto colour=text.startsWith("AUDIO ACTIVE")?juce::Colour(0xff64cf63)
                    :text.startsWith("AUDIO ERROR")?juce::Colour(0xffdf6f65):juce::Colour(0xffc7aa72);
                g.setColour(colour);g.fillEllipse(r.getX(),r.getCentreY()-4,8,8);r.removeFromLeft(17);
            }
            else
            {
                g.setColour(separator().withAlpha(separatorOpacity));
                g.fillRect(r.getX(),r.getCentreY()-5,1.f,10.f);r.removeFromLeft(12);
            }
            const float width=juce::jmin(r.getWidth(),juce::GlyphArrangement::getStringWidth(ui::font(statusSize),text)+12.f);
            g.setColour(i==0?ink():secondaryInk());g.drawText(text,r.removeFromLeft(width),juce::Justification::centredLeft,true);
        }
    }
};
}
