#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

namespace tonegrid::ampui
{
// UI-only piecewise mapping: asymmetric dB ranges still put zero at 12 o'clock.
// The stored parameter remains dB; NAM DSP/control/state code is unchanged.
inline juce::NormalisableRange<double> bipolarDbRange(double minimum,double maximum)
{
    juce::NormalisableRange<double> range(minimum,maximum,
        [](double low,double high,double position){return position<=.5?low+(0-low)*(position*2):high*((position-.5)*2);},
        [](double low,double high,double value){return value<=0?.5*(value-low)/(0-low):.5+.5*value/high;});
    range.interval=.1;return range;
}
inline void configure(juce::Slider& slider,double minimum,double maximum)
{
    slider.setNormalisableRange(bipolarDbRange(minimum,maximum));
    slider.setDoubleClickReturnValue(true,0);
    slider.setRotaryParameters(juce::MathConstants<float>::pi*1.25f,juce::MathConstants<float>::pi*2.75f,true);
}
}
