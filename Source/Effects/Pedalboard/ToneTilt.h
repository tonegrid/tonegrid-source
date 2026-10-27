#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace board
{
// First-order tilt, bilinear transform prewarped at 1 kHz:
// H(s) = (g*s + w) / (s + w*g). DC = 1/g, HF = g,
// |H(j*w)| = 1. At g=1 the output is exactly the input.
// TPT state remains current at neutral; no allocation or coefficient pointers
// are changed on the audio thread. Gain ramps avoid control discontinuities.
class ToneTilt
{
public:
    void prepare(double sampleRate, float db = 0)
    {
        warp = std::tan(juce::MathConstants<double>::pi * 1000.0 / sampleRate);
        gain.reset(sampleRate, .02);
        gain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain((double) db));
        reset();
    }
    void reset() { state = 0; }
    void setTone(float db) { gain.setTargetValue(juce::Decibels::decibelsToGain((double) juce::jlimit(-6.f, 6.f, db))); }
    float process(float input)
    {
        const double g = gain.getNextValue();
        const double a = warp * g / (1.0 + warp * g);
        const double v = (input - state) * a;
        const double low = v + state;
        state = low + v;
        return g == 1.0 ? input : (float) (g * input + (1.0 / g - g) * low);
    }
private:
    double warp = 0, state = 0;
    juce::SmoothedValue<double> gain;
};
}
