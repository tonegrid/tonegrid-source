#pragma once
#include <array>
#include <memory>
namespace tonegrid
{
// ToneGrid-owned wet-only adapter to AELAPSE's dedicated spring DSP.
// GPL-3.0 upstream and modification records are under vendor/aelapse-spring.
// All methods after prepare are audio-thread only. No UI, NAM, plugin or OS API.
class SpringReverbProcessor final
{
public:
    SpringReverbProcessor();
    ~SpringReverbProcessor();
    SpringReverbProcessor(const SpringReverbProcessor&)=delete;
    SpringReverbProcessor& operator=(const SpringReverbProcessor&)=delete;
    void prepare(double sampleRate, int maximumBlockSize);
    void reset() noexcept;
    bool setParameters(float timeSeconds, float color) noexcept;
    std::array<float,2> processSample(float left, float right) noexcept;
    bool isPrepared() const noexcept;
    size_t preparationAllocations() const noexcept;
    static constexpr int wetFrameDelaySamples=32;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
