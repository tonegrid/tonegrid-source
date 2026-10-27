#pragma once
#include "AmpNormalizationProcessor.h"
#include "Effects/AmpEq/AmpEqProcessor.h"
#include "Effects/StereoSpread/StereoSpreadProcessor.h"
#include "Effects/Reverb/ReverbProcessor.h"
#include "OutputDspParameters.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "ParameterMailbox.h"
namespace tonegrid::output
{
// Thin graph adapters only. The two DSP engines remain independent classes.
class NodeBase:public juce::AudioProcessor
{
public:
    explicit NodeBase(juce::String name):AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),label(std::move(name)){}
    const juce::String getName() const override{return label;}
    bool acceptsMidi() const override{return false;}bool producesMidi() const override{return false;}
    double getTailLengthSeconds() const override{return 0;}
    int getNumPrograms() override{return 1;}int getCurrentProgram() override{return 0;}void setCurrentProgram(int) override{}
    const juce::String getProgramName(int) override{return {}; }void changeProgramName(int,const juce::String&) override{}
    bool hasEditor() const override{return false;}juce::AudioProcessorEditor* createEditor() override{return nullptr;}
    bool isBusesLayoutSupported(const BusesLayout& layout) const override
    {const auto channels=layout.getMainOutputChannelSet().size();return layout.getMainInputChannelSet()==layout.getMainOutputChannelSet()&&(channels==1||channels==2);}
    void releaseResources() override{}
    // Rig codec owns these plain controls, not opaque plugin state.
    void getStateInformation(juce::MemoryBlock&) override{}void setStateInformation(const void*,int) override{}
private:juce::String label;
};
class NormalizationNode final:public NodeBase
{
public:
    NormalizationNode():NodeBase("ToneGrid Amp Normalization"){}
    void configure(bool enabled,bool nam,std::optional<float> metadata,float trimDb=0.f) noexcept
    {settings.publish({enabled,nam,metadata.value_or(NAN),trimDb});}
    void prepareToPlay(double rate,int) override{dsp.prepare(rate);}
    void processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi) override
    {
        juce::ScopedNoDenormals scoped;midi.clear();const auto p=settings.read();const auto db=p.loudness;
        dsp.setTarget(std::isfinite(db)?std::optional<float>{db}:std::nullopt,p.enabled,p.nam,p.trimDb);dsp.process(buffer);
    }
private:
    static_assert(std::atomic<float>::is_always_lock_free&&std::atomic<bool>::is_always_lock_free);
    struct Parameters{bool enabled=false,nam=false;float loudness=NAN,trimDb=0.f;};ParameterMailbox<Parameters> settings;
    AmpNormalizationProcessor dsp;
};
class AmpEqNode final:public NodeBase
{
public:
    AmpEqNode():NodeBase("ToneGrid NAM Amp EQ"){}
    bool configure(const ampEq::Parameters& p,bool nam) noexcept
    {if(!ampEq::valid(p))return false;settings.publish({p,nam});return true;}
    void prepareToPlay(double rate,int) override{dsp.prepare(rate);}
    void processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi) override
    {midi.clear();const auto p=settings.read();dsp.setTarget(p.eq,p.nam);dsp.process(buffer);}
private:
    struct Parameters{ampEq::Parameters eq;bool nam=false;};ParameterMailbox<Parameters> settings;ampEq::Processor dsp;
};
class SpreadNode final:public NodeBase
{
public:
    SpreadNode():NodeBase("ToneGrid Stereo Spread"){}
    bool configure(const Settings& s) noexcept
    {
        if(!valid(s))return false;
        settings.publish(s);return true;
    }
    void stereoOutputAvailable(bool stereo) noexcept{outputStereo.store(stereo);}
    void prepareToPlay(double rate,int block) override{dsp.prepare(rate,block);}
    void processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi) override
    {
        juce::ScopedNoDenormals scoped;midi.clear();
        if(!outputStereo.load()||buffer.getNumChannels()<2){dsp.forceIdle();return;}
        const auto state=settings.read();
        dsp.setTarget({state.offsetMs,state.wobbleEnabled?state.wobblePercent*.01f:0.f,state.crossoverHz,state.crossoverEnabled,state.diffuseEnabled},state.spreadEnabled);dsp.process(buffer);
    }
private:
    static_assert(std::atomic<float>::is_always_lock_free&&std::atomic<unsigned>::is_always_lock_free&&std::atomic<bool>::is_always_lock_free);
    ParameterMailbox<Settings> settings;std::atomic<bool> outputStereo{true};
    StereoSpreadProcessor dsp;
};
class ReverbNode final:public NodeBase
{
public:
    ReverbNode():NodeBase("ToneGrid Reverb"){}
    void stereoOutputAvailable(bool stereo) noexcept{outputStereo.store(stereo);}
    bool configure(const reverb::Parameters& p) noexcept
    {
        if(!reverb::valid(p))return false;
        settings.publish(p);return true;
    }
    void prepareToPlay(double rate,int block) override{dsp.prepare(rate,block);}
    void processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi) override
    {
        midi.clear();const auto p=settings.read();
        dsp.setTarget(p);dsp.process(buffer,outputStereo.load());
    }
    double getTailLengthSeconds() const override{return 20.;}
private:
    static_assert(std::atomic<float>::is_always_lock_free&&std::atomic<unsigned>::is_always_lock_free&&std::atomic<bool>::is_always_lock_free);
    ParameterMailbox<reverb::Parameters> settings;
    std::atomic<bool> outputStereo{true};
    ReverbProcessor dsp;
};
}
