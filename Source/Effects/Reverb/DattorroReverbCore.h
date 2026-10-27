#pragma once
#include <array>
#include <vector>
#include <algorithm>
#include <cmath>
#include <stdexcept>
extern "C" {
#include "../../../vendor/dattorro-verb/verb.h"
#include "../../../vendor/dattorro-verb/verb_structs.h"
}
namespace tonegrid::reverb::detail
{
// Delay/tap integers from pinned upstream initialize(). Storage ownership and
// sample-rate scaling live here; upstream processing code is unmodified.
struct DattorroCore
{
    DattorroVerb tank{};
    std::array<std::vector<double>,13> memory;
    std::array<DelayBuffer*,13> lines()
    {return {&tank.preDelay,&tank.inDiffusion[0],&tank.inDiffusion[1],&tank.inDiffusion[2],&tank.inDiffusion[3],
        &tank.decayDiffusion1[0],&tank.preDampingDelay[0],&tank.decayDiffusion2[0],&tank.postDampingDelay[0],
        &tank.decayDiffusion1[1],&tank.preDampingDelay[1],&tank.decayDiffusion2[1],&tank.postDampingDelay[1]};}
    void prepare(double rate)
    {
        rateScale=rate/29761.;
        constexpr std::array<std::array<int,4>,13> taps{{{0,0,0,0},{142,0,0,0},{107,0,0,0},{379,0,0,0},{277,0,0,0},
            {672,0,0,0},{4453,353,3627,1990},{1800,187,1228,0},{3720,1066,2673,0},
            {908,0,0,0},{4217,266,2974,2111},{2656,335,1913,0},{3163,121,1996,0}}};
        const auto buffers=lines();
        for(size_t i=0;i<buffers.size();++i)
        {
            std::array<unsigned,4> scaled{};
            for(size_t j=0;j<4;++j)scaled[j]=unsigned(std::lround(taps[i][j]*rate/29761.));
            // Original modulation moves 16 samples, so leave headroom at all rates.
            size_t size=2;while(size<=scaled[0]+32)size*=2;
            if(size>65536)throw std::invalid_argument("Dattorro delay exceeds 16-bit upstream capacity");
            memory[i].assign(size,0.);auto& line=*buffers[i];line.buffer=memory[i].data();line.mask=uint16_t(size-1);
            for(size_t j=0;j<4;++j)line.readOffset[j]=uint16_t(size-scaled[j]);
        }
        reset();
    }
    void reset() noexcept
    {
        for(auto& buffer:memory)std::fill(buffer.begin(),buffer.end(),0.);
        tank.preFilter=0;tank.damping[0]=tank.damping[1]=0;tank.t=0;
        // Reset the upstream's modulation drift, without altering other tap offsets.
        const double scale=rateScale;
        tank.decayDiffusion1[0].readOffset[0]=uint16_t(unsigned(tank.decayDiffusion1[0].mask)+1-unsigned(std::lround(672*scale)));
        tank.decayDiffusion1[1].readOffset[0]=uint16_t(unsigned(tank.decayDiffusion1[1].mask)+1-unsigned(std::lround(908*scale)));
    }
    void configure(float seconds,float brightness,float type,double rate) noexcept
    {
        const auto blend=[](float a,float b,float c,float t){return t<=1?a+(b-a)*t:b+(c-b)*(t-1);};
        const float decayTime=seconds*blend(.8f,1.f,1.2f,type);
        const float cutoff=blend(4500.f,6500.f,5000.f,type)*std::pow(2.f,(brightness-50.f)/35.f);
        const auto onePole=[&](float hz){return 1-std::exp(-6.2831853071795864769*hz/rate);};
        DattorroVerb_setPreFilter(&tank,onePole(std::min(18000.f,cutoff*1.7f)));
        DattorroVerb_setDamping(&tank,onePole(cutoff));
        DattorroVerb_setInputDiffusion1(&tank,blend(.55f,.75f,.80f,type));
        DattorroVerb_setInputDiffusion2(&tank,blend(.40f,.625f,.68f,type));
        DattorroVerb_setDecayDiffusion(&tank,blend(.55f,.70f,.74f,type));
        DattorroVerb_setDecay(&tank,std::clamp(std::exp(std::log(.001)*.35/(2*decayTime)),.1,.97));
    }
    double rateScale=48000./29761.;
};
}
