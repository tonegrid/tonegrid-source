// SPDX-License-Identifier: MIT
// Static-gain behavior adapted from TONE3000 Processor.cpp.
// Copyright (c) 2026 TONE3000. See Resources/Licenses/TONE3000-MIT.txt.
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <optional>

namespace tonegrid
{
// Audio-thread DSP only. Metadata decoding and UI/control publication are external.
// No detector, compressor, limiter, calibration or per-pedal normalization.
class AmpNormalizationProcessor
{
public:
    static constexpr float targetLoudnessDb=-18.f;
    static float adjustmentDb(std::optional<float> loudness,float trimDb=0.f) noexcept
    {
        // Keep the historical metadata correction, then add the optional measured
        // ToneGrid trim. Do not clamp the combined value back to +/-12 dB.
        const auto base=loudness&&std::isfinite(*loudness)&&*loudness>=-100.f&&*loudness<=0.f
            ?juce::jlimit(-12.f,12.f,targetLoudnessDb-*loudness):0.f;
        return base+(std::isfinite(trimDb)&&trimDb>=-24.f&&trimDb<=24.f?trimDb:0.f);
    }
    void prepare(double sampleRate) noexcept
    {
        gain.reset(std::isfinite(sampleRate)&&sampleRate>0?sampleRate:48000,.025);
        gain.setCurrentAndTargetValue(1.f);
    }
    void setTarget(std::optional<float> loudness,bool enabled,bool namAmp,float trimDb=0.f) noexcept
    {
        // Source replacement already detaches audio. Never carry NAM gain into VST.
        if(!namAmp){gain.setCurrentAndTargetValue(1.f);return;}
        gain.setTargetValue(enabled?juce::Decibels::decibelsToGain(adjustmentDb(loudness,trimDb)):1.f);
    }
    void process(juce::AudioBuffer<float>& buffer) noexcept
    {
        // A settled bypass is bit-exact, including signed zero and stereo input.
        if(!gain.isSmoothing()&&gain.getCurrentValue()==1.f)return;
        for(int sample=0;sample<buffer.getNumSamples();++sample)
        {
            const auto g=gain.getNextValue();
            for(int channel=0;channel<buffer.getNumChannels();++channel)
                buffer.getWritePointer(channel)[sample]*=g;
        }
    }
    float currentGain() const noexcept{return gain.getCurrentValue();}
private:
    juce::LinearSmoothedValue<float> gain{1.f};
};
}
