#include "Effects/Reverb/ReverbProcessor.h"
#include <array>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include "Effects/Reverb/DattorroReverbCore.h"
#include "Effects/Reverb/SpringReverbProcessor.h"
namespace tonegrid
{
namespace
{
struct Glide
{
    float value=0,target=0,coefficient=0;
    void prepare(double rate,double seconds,float v) noexcept{coefficient=float(1-std::exp(-1/(rate*seconds)));value=target=v;}
    float next() noexcept{value+=(target-value)*coefficient;if(std::abs(target-value)<1.e-5f)value=target;return value;}
};
}
struct ReverbProcessor::Impl
{
    reverb::detail::DattorroCore dattorro;
    SpringReverbProcessor spring;
    std::array<std::vector<float>,2> predelay;
    size_t write=0;
    double rate=48000;
    bool prepared=false;
    reverb::Parameters target;
    Glide time,color,delay,character;
    juce::SmoothedValue<float> springBlend;
    float lastDattorroType=0;
    juce::SmoothedValue<float> mix,engage;
    unsigned controlCounter=0;
    void initializeControls() noexcept
    {
        time.prepare(rate,.05,target.timeSeconds);color.prepare(rate,.05,target.color);
        if(target.type!=reverb::Type::Spring)lastDattorroType=float(target.type);
        delay.prepare(rate,.1,target.preDelayMs);character.prepare(rate,.1,lastDattorroType);
        springBlend.reset(rate,.05);springBlend.setCurrentAndTargetValue(target.type==reverb::Type::Spring?1.f:0.f);
        mix.reset(rate,.025);mix.setCurrentAndTargetValue(target.mixPercent*.01f);
        engage.reset(rate,.025);engage.setCurrentAndTargetValue(0);engage.setTargetValue(target.enabled?1.f:0.f);
        controlCounter=0;
    }
    void parameters(float seconds,float brightness,float type) noexcept
    {
        dattorro.configure(seconds,brightness,type,rate);
        spring.setParameters(seconds,brightness);
    }
    float delayed(int channel,float input,float ms) noexcept
    {
        auto& ring=predelay[size_t(channel)];ring[write]=input;
        double position=double(write)-ms*rate*.001;if(position<0)position+=double(ring.size());
        const auto a=size_t(position);const float fraction=float(position-double(a));
        return ring[a]+fraction*(ring[(a+1)%ring.size()]-ring[a]);
    }
};
ReverbProcessor::ReverbProcessor():impl(std::make_unique<Impl>()){}
ReverbProcessor::~ReverbProcessor()=default;
bool ReverbProcessor::isPrepared() const noexcept{return impl->prepared;}
void ReverbProcessor::prepare(double rate,int block)
{
    if(!std::isfinite(rate)||rate<8000||rate>192000||block<1)throw std::invalid_argument("Unsupported reverb preparation");
    auto& s=*impl;s.prepared=false;s.rate=rate;s.dattorro.rateScale=rate/29761.;s.dattorro.prepare(rate);
    s.spring.prepare(rate,block);
    for(auto& ring:s.predelay)ring.assign(size_t(std::ceil(rate*.1))+2,0.f);
    s.prepared=true;reset();
}
void ReverbProcessor::reset() noexcept
{
    auto& s=*impl;if(!s.prepared)return;
    s.dattorro.reset();s.spring.setParameters(s.target.timeSeconds,s.target.color);s.spring.reset();for(auto& ring:s.predelay)std::fill(ring.begin(),ring.end(),0.f);
    s.write=0;s.initializeControls();s.parameters(s.target.timeSeconds,s.target.color,s.lastDattorroType);
}
bool ReverbProcessor::setTarget(const reverb::Parameters& target) noexcept
{
    if(!reverb::valid(target))return false;
    auto& s=*impl;s.target=target;
    s.time.target=target.timeSeconds;s.color.target=target.color;s.delay.target=target.preDelayMs;
    if(target.type!=reverb::Type::Spring)s.lastDattorroType=float(target.type);
    s.character.target=s.lastDattorroType;
    s.springBlend.setTargetValue(target.type==reverb::Type::Spring?1.f:0.f);
    s.mix.setTargetValue(target.mixPercent*.01f);s.engage.setTargetValue(target.enabled?1.f:0.f);return true;
}
void ReverbProcessor::process(juce::AudioBuffer<float>& buffer,bool stereoOutput) noexcept
{
    auto& s=*impl;if(!s.prepared||buffer.getNumChannels()<1)return;
    juce::ScopedNoDenormals noDenormals;
    auto* left=buffer.getWritePointer(0);auto* right=buffer.getNumChannels()>1?buffer.getWritePointer(1):nullptr;
    for(int i=0;i<buffer.getNumSamples();++i)
    {
        const auto seconds=s.time.next(),brightness=s.color.next(),type=s.character.next();
        // Setters are scalar/no-allocation; quantize coefficient updates to 32 samples.
        if((s.controlCounter++&31u)==0)s.parameters(seconds,brightness,type);
        const float engaged=s.engage.getNextValue(),amount=engaged*s.mix.getNextValue();
        const float dryL=left[i],dryR=right?right[i]:dryL;
        const float delayMs=s.delay.next();
        // Engines always clock, including bypass, with no new excitation when OFF.
        // This drains tails and preserves smooth Dattorro/Spring handoffs.
        const float excitation=engaged;
        float wetL=s.delayed(0,dryL*excitation,delayMs),wetR=s.delayed(1,dryR*excitation,delayMs);
        s.write=(s.write+1)%s.predelay[0].size();
        DattorroVerb_process(&s.dattorro.tank,(double(wetL)+wetR)*.5);
        const float aL=float(DattorroVerb_getLeft(&s.dattorro.tank))*.35f,aR=float(DattorroVerb_getRight(&s.dattorro.tank))*.35f;
        const auto springWet=s.spring.processSample(wetL,wetR);
        wetL=aL;wetR=aR;
        const float springAmount=s.springBlend.getNextValue();
        if(springAmount==1){wetL=springWet[0];wetR=springWet[1];}
        else if(springAmount!=0){wetL+=springAmount*(springWet[0]-wetL);wetR+=springAmount*(springWet[1]-wetR);}
        // Stable bypass/Mix 0 performs no arithmetic/store on the original samples.
        if(amount!=0)
        {
            if(!stereoOutput){const auto monoWet=(wetL+wetR)*.5f;wetL=wetR=monoWet;}
            if(right){left[i]=dryL+amount*(wetL-dryL);right[i]=dryR+amount*(wetR-dryR);}
            else left[i]=dryL+amount*((wetL+wetR)*.5f-dryL);
        }
    }
}
}
