#pragma once
#include "EffectsParameters.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>
namespace tonegrid
{
class ModProcessor
{
public:
    ModProcessor();~ModProcessor();
    void prepare(double sampleRate,int maximumBlock);
    void reset() noexcept;
    void setTarget(const mod::Parameters&) noexcept;
    void process(juce::AudioBuffer<float>&) noexcept;
private:struct Impl;std::unique_ptr<Impl> impl;
};
}
