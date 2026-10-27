#include "Effects/Reverb/SpringReverbProcessor.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include "../../../vendor/aelapse-spring/processors/springs/Springs.h"
namespace tonegrid
{
struct SpringReverbProcessor::Impl
{
    struct Release { void operator()(processors::Springs* core) const noexcept { if(core){core->free();delete core;} } };
    std::unique_ptr<processors::Springs,Release> core;
    std::array<std::array<float,wetFrameDelaySamples>,2> input{},output{};
    int position=0;
    double rate=48000;
    float time=1.5f,color=40;
    size_t allocations=0;
    void controls() noexcept
    {
        if(!core)return;
        // One conservative voicing, independent of tape delay. Keep the dispersive
        // cutoff fixed so Color never switches the kernel's decimation factor.
        core->update(.62f,float(std::min(4500.,rate*.35)),.05f,
                     std::max(.3f,time),.30f+.45f*color*.01f,
                     .10f,.50f,.35f,1.f,wetFrameDelaySamples);
    }
};
SpringReverbProcessor::SpringReverbProcessor():impl(std::make_unique<Impl>()){}
SpringReverbProcessor::~SpringReverbProcessor()=default;
bool SpringReverbProcessor::isPrepared() const noexcept{return impl->core!=nullptr;}
size_t SpringReverbProcessor::preparationAllocations() const noexcept{return impl->allocations;}
void SpringReverbProcessor::prepare(double rate,int block)
{
    if(!std::isfinite(rate)||rate<8000||rate>192000||block<1)throw std::invalid_argument("Unsupported spring preparation");
    auto& s=*impl;s.core.reset();s.rate=rate;s.allocations=0;
    s.core.reset(new processors::Springs());
    s.core->prepare(float(rate),wetFrameDelaySamples,[&](void* previous,size_t bytes)->void*
    {++s.allocations;auto* p=std::realloc(previous,bytes);if(!p)throw std::bad_alloc();return p;});
    s.controls();reset();
}
void SpringReverbProcessor::reset() noexcept
{
    auto& s=*impl;if(s.core)s.core->reset();s.input={};s.output={};s.position=0;
}
bool SpringReverbProcessor::setParameters(float time,float color) noexcept
{
    if(!std::isfinite(time)||time<.2f||time>10.f||!std::isfinite(color)||color<0||color>100)return false;
    auto& s=*impl;s.time=time;s.color=color;s.controls();return true;
}
std::array<float,2> SpringReverbProcessor::processSample(float left,float right) noexcept
{
    auto& s=*impl;if(!s.core)return {};
    const size_t i=size_t(s.position);const std::array<float,2> result{s.output[0][i]*.7f,s.output[1][i]*.7f};
    s.input[0][i]=left;s.input[1][i]=right;
    if(++s.position==wetFrameDelaySamples)
    {
        const float* inputs[]{s.input[0].data(),s.input[1].data()};float* outputs[]{s.output[0].data(),s.output[1].data()};
        s.core->process(inputs,outputs,wetFrameDelaySamples);s.position=0;
    }
    return result;
}
}
