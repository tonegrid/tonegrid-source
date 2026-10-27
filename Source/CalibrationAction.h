#pragma once
#include "Shared/UI/UiMotion138.h"

namespace tonegrid::calibration
{
class Action final : public juce::TextButton, private juce::Timer
{
public:
    Action():TextButton("Calibration")
    {l10n::button(*this,l10n::Language::instance().source("calibration.short"));l10n::tooltip(*this,"Guitar Calibration");setTitle("Guitar Calibration");getProperties().set("calibration-attention",true);for(int i=0;i<3;++i)fade.advance(true,.1);startTimerHz(60);}
    std::function<bool()> nearby;
    void setNeedsAttention(bool value)
    {
        if(attention==value)return;attention=value;
        getProperties().set("calibration-attention",value);repaint();
    }
    bool needsAttention() const{return attention;}
    void advance(bool pointerNearby,double dt,double seconds)
    {
        const auto value=fade.advance(attention||pointerNearby,dt);setAlpha(value);
        const auto pulse=.25f+.75f*float(.5-.5*std::cos(seconds*juce::MathConstants<double>::twoPi/1.5));
        if(attention){getProperties().set("calibration-pulse",pulse);repaint();}
    }
private:
    void timerCallback() override
    {
        const auto now=juce::Time::getMillisecondCounterHiRes();
        if(isShowing())advance(nearby&&nearby(),last==0?1./60.:(now-last)/1000.,now/1000.);
        last=now;
    }
    motion::CardFade fade;bool attention=true;double last=0;
};
}
