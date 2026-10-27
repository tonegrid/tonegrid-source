#pragma once
#include "Shared/UI/UiStyle.h"

namespace tonegrid::ui
{
// Shared read-only help renderer. Measurement and painting use the same layout,
// so longer translations can grow a popup instead of being squeezed or clipped.
class ControlHelp final:public juce::Label
{
public:
    ControlHelp(){setFont(ui::font(16));setJustificationType(juce::Justification::topLeft);setMinimumHorizontalScale(1.f);}
    void setCopy(const juce::String& source){l10n::text(*this,source);}
    int contentHeight(int width)const
    {juce::TextLayout layout;makeLayout(layout,width);return juce::roundToInt(std::ceil(layout.getHeight()))+8;}
    void paint(juce::Graphics& g)override
    {juce::TextLayout layout;makeLayout(layout,getWidth());layout.draw(g,getLocalBounds().toFloat().reduced(4));}
private:
    void makeLayout(juce::TextLayout& layout,int width)const
    {juce::AttributedString copy;copy.append(getText(),getFont(),findColour(juce::Label::textColourId));layout.createLayout(copy,float(juce::jmax(1,width-8)));}
};
}
