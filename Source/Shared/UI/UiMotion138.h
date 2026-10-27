#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

namespace tonegrid::motion
{
inline float ledOpacity(double seconds) noexcept
{return .875f+.125f*float(std::cos(seconds*juce::MathConstants<double>::twoPi/3.));}
// Ten quiet seconds, then a two-second white/grey/white breath.
inline float tabAccent(double seconds) noexcept
{
    const auto phase=std::fmod(juce::jmax(0.,seconds),12.);
    return phase<10.?0.f:float(std::pow(std::sin((phase-10.)*juce::MathConstants<double>::pi/2.),2.));
}
inline float fade(float value,bool visible,double dt) noexcept
{
    const float target=visible?1.f:0.f;
    const auto next=value+(target-value)*float(1.-std::exp(-juce::jlimit(0.,.1,dt)/.085));
    return std::abs(next-target)<.005f?target:next;
}
// A bounded, time-based ease for the two rack cards. Reversing direction starts
// at the current opacity, and endpoints settle exactly without a late snap.
class CardFade final
{
public:
    float advance(bool show,double dt) noexcept
    {
        if(show!=target){start=value;elapsed=0;target=show;}
        const double duration=target?.24:.30;
        elapsed=juce::jmin(duration,elapsed+juce::jlimit(0.,.1,dt));
        const auto t=float(elapsed/duration),ease=t*t*(3.f-2.f*t);
        value=start+((target?1.f:0.f)-start)*ease;return value;
    }
private:float value=0,start=0;double elapsed=0;bool target=false;
};
class HoverValueSlider final:public juce::Slider
{
public:
    void advance(bool nearby,double dt)
    {
        const auto next=fade(opacity,nearby||isMouseButtonDown()||hasKeyboardFocus(true),dt);
        if(next!=opacity){opacity=next;applyOpacity();}
    }
    float readoutOpacity()const noexcept{return opacity;}
    void resized()override{juce::Slider::resized();applyOpacity();}
private:
    void applyOpacity()
    {
        for(auto* child:getChildren())
            if(auto* label=dynamic_cast<juce::Label*>(child))label->setAlpha(label->isBeingEdited()?1.f:opacity);
    }
    float opacity=0.f;
};
}
