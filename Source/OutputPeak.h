#pragma once
#include <atomic>
#include <cmath>

namespace tonegrid
{
// Device callback writes; the UI consumes the maximum since its previous tick.
// Includes both channels, tuner mute and recorder playback. No gain or limiter.
class OutputPeak final
{
public:
    void push(const float* const* channels,int count,int frames) noexcept
    {
        float maximum=0;
        for(int channel=0;channel<count;++channel)
            if(channels[channel])for(int sample=0;sample<frames;++sample)
            {
                const auto value=channels[channel][sample];
                const auto magnitude=std::isfinite(value)?std::abs(value):2.f;
                if(magnitude>maximum)maximum=magnitude;
            }
        auto previous=peak.load(std::memory_order_relaxed);
        while(previous<maximum&&!peak.compare_exchange_weak(previous,maximum,std::memory_order_relaxed)){}
    }
    float take() noexcept{return peak.exchange(0,std::memory_order_relaxed);}
    void reset() noexcept{peak.store(0,std::memory_order_relaxed);}
private:
    static_assert(std::atomic<float>::is_always_lock_free);
    std::atomic<float> peak{0};
};
}
