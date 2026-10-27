#pragma once
#include "EffectsParameters.h"
#include <memory>
#include <atomic>
#include "ParameterMailbox.h"
namespace tonegrid
{
// Mono input processor, after the gate and before the existing thirteen pedals.
// Third-party spectral APIs are confined to the implementation.
class PitchEngine
{
public:
    PitchEngine();~PitchEngine();
    void prepare(double sampleRate,int fftSize=2048,int hopSize=64,int synthesisSize=256);
    void reset() noexcept;
    bool configure(const pitch::Parameters&) noexcept;
    pitch::Parameters settings() const noexcept;
    void process(float* samples,int count) noexcept;
    int latencySamples() const noexcept;
private:
    static_assert(std::atomic<int>::is_always_lock_free&&std::atomic<float>::is_always_lock_free);
    struct Impl;std::unique_ptr<Impl> impl;
    std::atomic<int> mode{0},transpose{0};std::atomic<float> dry{100},down{50},up{0};
    ParameterMailbox<pitch::Parameters> published;
};
}
