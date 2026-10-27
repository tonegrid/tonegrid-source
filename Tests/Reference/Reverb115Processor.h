// Frozen pre-Spring comparison interface, 2026-10-01.
#pragma once
#include "Effects/Reverb/ReverbParameters.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>
namespace tonegrid::reference
{
// Historical development selector retained only in the frozen test oracle.
enum class Candidate { Dattorro, Juce };
// Owns both prepared candidates. No UI, source/amp identity or platform APIs.
// Control methods/process are audio-thread only; the graph adapter publishes atomics.
class ReverbProcessor final
{
public:
    ReverbProcessor();
    ~ReverbProcessor();
    ReverbProcessor(const ReverbProcessor&)=delete;
    ReverbProcessor& operator=(const ReverbProcessor&)=delete;
    void prepare(double sampleRate,int maximumBlockSize);
    void reset() noexcept;
    bool setTarget(const reverb::Parameters&,Candidate) noexcept;
    void process(juce::AudioBuffer<float>&,bool stereoOutput=true) noexcept;
    bool isPrepared() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
