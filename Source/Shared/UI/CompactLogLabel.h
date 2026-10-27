#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
namespace tonegrid
{
// No background or frame; one line with ellipsis, never wrapping or shrinking.
class CompactLogLabel final:public juce::Label
{
public:
    void paint(juce::Graphics& g) override
    {g.setFont(getFont());g.setColour(findColour(juce::Label::textColourId));g.drawText(getText(),getLocalBounds().reduced(4,1),juce::Justification::centredRight,true);}
};
}
