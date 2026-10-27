// Frozen pre-Spring DSP, namespace/include changes only, 2026-10-01.
#include "Reverb115Processor.h"
#include <array>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include "Effects/Reverb/DattorroReverbCore.h"
namespace tonegrid::reference
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
    juce::Reverb baseline;
    std::array<std::vector<float>,2> predelay;
    size_t write=0;
    double rate=48000;
    bool prepared=false;
    reverb::Parameters target;
    Candidate candidate=Candidate::Dattorro;
    Glide time,color,delay,character,engine;
    juce::SmoothedValue<float> mix,engage;
    unsigned controlCounter=0;
    void initializeControls() noexcept
    {
        time.prepare(rate,.05,target.timeSeconds);color.prepare(rate,.05,target.color);
        delay.prepare(rate,.1,target.preDelayMs);character.prepare(rate,.1,float(target.type));engine.prepare(rate,.05,float(candidate));
        mix.reset(rate,.025);mix.setCurrentAndTargetValue(target.mixPercent*.01f);
        engage.reset(rate,.025);engage.setCurrentAndTargetValue(0);engage.setTargetValue(target.enabled?1.f:0.f);
        controlCounter=0;
    }
    void parameters(float seconds,float brightness,float type) noexcept
    {
        const auto blend=[](float a,float b,float c,float t){return t<=1?a+(b-a)*t:b+(c-b)*(t-1);};
        const float decayTime=seconds*blend(.8f,1.f,1.2f,type);
        dattorro.configure(seconds,brightness,type,rate);
        juce::Reverb::Parameters p;
        p.roomSize=std::clamp((float(std::exp(std::log(.001)*.035/decayTime))-.7f)/.28f,0.f,1.f);
        p.damping=std::clamp((100-brightness)*.009f+blend(.08f,0.f,.12f,type),0.f,1.f);
        p.width=blend(.65f,1.f,1.f,type);p.wetLevel=1.f/3.f;p.dryLevel=0;p.freezeMode=0;
        baseline.setParameters(p);
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
    s.baseline.setSampleRate(rate);
    for(auto& ring:s.predelay)ring.assign(size_t(std::ceil(rate*.1))+2,0.f);
    s.prepared=true;reset();
}
void ReverbProcessor::reset() noexcept
{
    auto& s=*impl;if(!s.prepared)return;
    s.dattorro.reset();s.baseline.reset();for(auto& ring:s.predelay)std::fill(ring.begin(),ring.end(),0.f);
    s.write=0;s.initializeControls();s.parameters(s.target.timeSeconds,s.target.color,float(s.target.type));
}
bool ReverbProcessor::setTarget(const reverb::Parameters& target,Candidate candidate) noexcept
{
    if(!reverb::valid(target)||int(candidate)<0||int(candidate)>1)return false;
    auto& s=*impl;s.target=target;s.candidate=candidate;
    s.time.target=target.timeSeconds;s.color.target=target.color;s.delay.target=target.preDelayMs;
    s.character.target=float(target.type);s.engine.target=float(candidate);
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
        // This drains tails naturally and keeps development A/B equally warm.
        const float excitation=engaged;
        float wetL=s.delayed(0,dryL*excitation,delayMs),wetR=s.delayed(1,dryR*excitation,delayMs);
        s.write=(s.write+1)%s.predelay[0].size();
        DattorroVerb_process(&s.dattorro.tank,(double(wetL)+wetR)*.5);
        const float aL=float(DattorroVerb_getLeft(&s.dattorro.tank))*.35f,aR=float(DattorroVerb_getRight(&s.dattorro.tank))*.35f;
        s.baseline.processStereo(&wetL,&wetR,1);
        const float engine=s.engine.next();wetL=aL+(wetL-aL)*engine;wetR=aR+(wetR-aR)*engine;
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
