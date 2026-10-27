#pragma once
#include "OutputDspNodes.h"
#include "Effects/Modulation/ModProcessor.h"
#include "Effects/Delay/DigitalDelay.h"
namespace tonegrid::output
{
class ModNode final:public NodeBase
{
public:
    ModNode():NodeBase("ToneGrid MOD"){}
    bool configure(const mod::Parameters& p) noexcept
    {if(!mod::valid(p))return false;settings.publish(p);return true;}
    void prepareToPlay(double sr,int block) override {dsp.prepare(sr,block);}
    void processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer& m) override
    {m.clear();dsp.setTarget(settings.read());dsp.process(b);}
private:
    static_assert(std::atomic<float>::is_always_lock_free&&std::atomic<unsigned>::is_always_lock_free);
    ParameterMailbox<mod::Parameters> settings;ModProcessor dsp;
};
class DelayNode final:public NodeBase
{
public:
    DelayNode():NodeBase("ToneGrid Digital Delay"){}
    bool configure(const delay::Parameters& p) noexcept
    {if(!delay::valid(p))return false;settings.publish(p);return true;}
    void prepareToPlay(double sr,int block) override {dsp.prepare(sr,block);}
    void processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer& m) override
    {m.clear();dsp.setTarget(settings.read());dsp.process(b);}
    double getTailLengthSeconds() const override {return 60.;}
private:
    static_assert(std::atomic<float>::is_always_lock_free&&std::atomic<bool>::is_always_lock_free);
    ParameterMailbox<delay::Parameters> settings;DigitalDelay dsp;
};
}
