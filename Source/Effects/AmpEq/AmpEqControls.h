#pragma once
#include "AmpChassis.h"
#include "Effects/AmpEq/AmpEqParameters.h"
#include <array>
namespace tonegrid::ampui
{
// Each complete row is a drag target; the value appears on hover and while dragging.
class EqControls final:public juce::Component
{
    class Knob final:public juce::Slider
    {
    public:
        Knob(){setSliderStyle(RotaryHorizontalVerticalDrag);setTextBoxStyle(NoTextBox,false,0,0);setRange(-12,12,.1);setDoubleClickReturnValue(true,0);setPopupDisplayEnabled(true,true,nullptr);setTextValueSuffix(" dB");setNumDecimalPlacesToDisplay(1);}
    };
public:
    EqControls()
    {
        const juce::StringArray names{"Bass","Mid","Treble"};
        for(size_t i=0;i<knobs.size();++i)
        {auto& knob=knobs[i];knob.setLookAndFeel(&look);knob.setTitle("NAM "+names[int(i)]);knob.setTooltip(names[int(i)]+": post-NAM EQ (-12 to +12 dB). Double-click to reset.");knob.onValueChange=[this]{if(onChange)onChange(state());};addAndMakeVisible(knob);}
    }
    ~EqControls() override{for(auto& knob:knobs)knob.setLookAndFeel(nullptr);}
    ampEq::Parameters state() const noexcept{return {float(knobs[0].getValue()),float(knobs[1].getValue()),float(knobs[2].getValue())};}
    void sync(const ampEq::Parameters& p){knobs[0].setValue(p.bassDb,juce::dontSendNotification);knobs[1].setValue(p.midDb,juce::dontSendNotification);knobs[2].setValue(p.trebleDb,juce::dontSendNotification);}
    void resized() override
    {auto area=getLocalBounds();for(int i=0;i<3;++i){auto row=area.removeFromTop(area.getHeight()/(3-i));knobs[size_t(i)].setBounds(row.withTrimmedLeft(39).reduced(0,1));}}
    void paint(juce::Graphics& g) override
    {g.setColour(juce::Colour(0xffd9e0e2));g.setFont(juce::FontOptions("Segoe UI",11.f,juce::Font::plain));const juce::StringArray names{"Bass","Mid","Treble"};for(int i=0;i<3;++i)g.drawText(names[i],0,i*getHeight()/3,39,getHeight()/3,juce::Justification::centredLeft);}
    std::function<void(ampEq::Parameters)> onChange;
private:Look look;std::array<Knob,3> knobs;
};
}
