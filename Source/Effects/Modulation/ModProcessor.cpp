#include "Effects/Modulation/ModProcessor.h"
#include <Effects/chorus.h>
#include <Effects/tremolo.h>
#include <Effects/phaser.h>
#include <Effects/flanger.h>
#include <Synthesis/oscillator.h>
#include <array>
namespace tonegrid
{
struct ModProcessor::Impl
{
    std::array<daisysp::ChorusEngine,2> chorus;
    std::array<daisysp::Tremolo,2> tremolo;
    std::array<daisysp::Phaser,2> phaser;
    std::array<daisysp::Flanger,2> flanger;
    juce::SmoothedValue<float> rate,depth,level,engage;
    mod::Parameters target;mod::Type active=mod::Type::Chorus;double sr=48000;
    void init()
    {
        for(int c=0;c<2;++c)
        {
            chorus[c].Init(float(sr));chorus[c].SetDelayMs(c==0?7.f:9.f);chorus[c].SetFeedback(.08f);
            tremolo[c].Init(float(sr));tremolo[c].SetWaveform(daisysp::Oscillator::WAVE_SIN);
            phaser[c].Init(float(sr));phaser[c].SetPoles(4);phaser[c].SetFreq(600);phaser[c].SetFeedback(.15f);
            flanger[c].Init(float(sr));flanger[c].SetDelayMs(2.5f);flanger[c].SetFeedback(.2f);
        }
    }
};
ModProcessor::ModProcessor()=default;ModProcessor::~ModProcessor()=default;
void ModProcessor::prepare(double sr,int)
{
    impl=std::make_unique<Impl>();impl->sr=sr;impl->init();
    for(auto* s:{&impl->rate,&impl->depth,&impl->level,&impl->engage})s->reset(sr,.025);
    impl->rate.setCurrentAndTargetValue(1);impl->depth.setCurrentAndTargetValue(.35f);impl->level.setCurrentAndTargetValue(.35f);impl->engage.setCurrentAndTargetValue(0);
}
void ModProcessor::reset() noexcept {if(impl){impl->init();impl->engage.setCurrentAndTargetValue(0);}}
void ModProcessor::setTarget(const mod::Parameters& p) noexcept
{if(!impl||!mod::valid(p))return;impl->target=p;impl->rate.setTargetValue(p.rateHz);impl->depth.setTargetValue(p.depth*.01f);impl->level.setTargetValue(p.level*.01f);}
void ModProcessor::process(juce::AudioBuffer<float>& b) noexcept
{
    if(!impl)return;auto& e=*impl;juce::ScopedNoDenormals scoped;
    const bool changing=e.target.type!=e.active;
    e.engage.setTargetValue(e.target.enabled&&!changing?1.f:0.f);
    if(!e.engage.isSmoothing()&&e.engage.getCurrentValue()==0)
    {
        if(changing){e.active=e.target.type;e.init();e.engage.setTargetValue(e.target.enabled?1.f:0.f);}
        else if(!e.target.enabled)return;
    }
    for(int i=0;i<b.getNumSamples();++i)
    {
        const float rate=e.rate.getNextValue(),d=e.depth.getNextValue(),amount=e.level.getNextValue()*e.engage.getNextValue();
        for(int c=0;c<juce::jmin(2,b.getNumChannels());++c)
        {
            const auto x=b.getSample(c,i);float effected=x;
            switch(e.active)
            {
            case mod::Type::Chorus:e.chorus[c].SetLfoFreq(rate*(c==0?1.f:1.07f));e.chorus[c].SetLfoDepth(d*.75f);effected=e.chorus[c].Process(x);break;
            case mod::Type::Tremolo:e.tremolo[c].SetFreq(rate);e.tremolo[c].SetDepth(d);effected=e.tremolo[c].Process(x);break;
            // Daisy's Phaser sums four parallel engines. Normalize the sum to unity
            // so selecting four poles cannot quadruple the user's output level.
            case mod::Type::Phaser:e.phaser[c].SetLfoFreq(rate);e.phaser[c].SetLfoDepth(d*.85f);effected=e.phaser[c].Process(x)*.25f;break;
            case mod::Type::Flanger:e.flanger[c].SetLfoFreq(rate);e.flanger[c].SetLfoDepth(d*.85f);effected=e.flanger[c].Process(x);break;
            }
            if(amount!=0)b.setSample(c,i,x+amount*(effected-x));
        }
    }
}
}
