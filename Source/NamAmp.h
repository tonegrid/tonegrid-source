#pragma once
#include "AmpSource.h"
#include "Content/Amps/NamAmpLibrary.h"
#include <atomic>
#include "NamQuality.h"
#include "ParameterMailbox.h"
#include "LevelCalibration.h"

namespace amp
{
// Dedicated amp processor, never part of board::Processor or its pedal IDs.
class NamAmpProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int chunk=128;
    NamAmpProcessor(std::unique_ptr<nam::DSP> model,const namamp::State& initial,bool lite=false,float inputCompensationDb=0.f)
        :AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),dsp(std::move(model)),captureInputGain(juce::Decibels::decibelsToGain(namamp::validatedInputCompensation(inputCompensationDb))),gainDb(initial.gainDb),volumeDb(initial.volumeDb)
    {
        // A private candidate may be prepared on the loading worker. Graph
        // publication and retirement remain on the message thread.
        published.publish({initial.gainDb,initial.volumeDb,1.f});
        dsp->SetPrewarmOnReset(false);dsp->Reset(48000,chunk);tonegrid::namquality::apply(*dsp,lite);dsp->prewarm();
        gain.reset(48000,.02);volume.reset(48000,.02);gain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(initial.gainDb)*captureInputGain);volume.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(initial.volumeDb));
    }
    const juce::String getName() const override{return "ToneGrid NAM Amp";}
    bool acceptsMidi() const override{return false;}bool producesMidi() const override{return false;}
    double getTailLengthSeconds() const override{return 0;}
    int getNumPrograms() override{return 1;}int getCurrentProgram() override{return 0;}void setCurrentProgram(int) override{}
    const juce::String getProgramName(int) override{return {}; }void changeProgramName(int,const juce::String&) override{}
    bool hasEditor() const override{return false;}juce::AudioProcessorEditor* createEditor() override{return nullptr;}
    bool isBusesLayoutSupported(const BusesLayout& layout) const override
    {return layout.getMainInputChannelSet()==juce::AudioChannelSet::stereo()&&layout.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();}
    // Reprepare changes bounded control state only, retaining neural history.
    // Heavy Reset/prewarm occurs once, before publication of each new instance.
    void prepareToPlay(double rate,int) override
    {rateOK.store(rate==48000);const auto old=gain.getCurrentValue();gain.reset(48000,.02);volume.reset(48000,.02);gain.setCurrentAndTargetValue(prepared?old:juce::Decibels::decibelsToGain(gainDb.load())*captureInputGain*calibrationGain);volume.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(volumeDb.load()));prepared=true;}
    void releaseResources() override{}
    bool controls(float drive,float level)
    {if(!namamp::validControls(drive,level))return false;gainDb.store(drive);volumeDb.store(level);published.publish({drive,level,calibrationGain});return true;}
    void setCalibration(tonegrid::calibration::Settings settings)
    {
        jassert(juce::MessageManager::existsAndIsCurrentThread());
        
        const auto info=tonegrid::calibration::metadata(*dsp);
        calibrationGain=tonegrid::calibration::active(settings)&&!invalidCalibration&&!info.invalid?tonegrid::calibration::inputGain(settings.referenceDbU,info.input):1.f;
        published.publish({gainDb.load(),volumeDb.load(),calibrationGain});
    }
    void setUpstreamReference(const tonegrid::calibration::ReferenceRouting* value) noexcept{upstreamReference.store(value,std::memory_order_release);}
    void rejectCalibration(bool invalid) noexcept{invalidCalibration=invalid;}
    float drive() const{return gainDb.load();}float level() const{return volumeDb.load();}
    bool supportsLite() const{return dynamic_cast<nam::SlimmableModel*>(dsp.get())!=nullptr;}
    void getStateInformation(juce::MemoryBlock& serialized) override
    {juce::MemoryOutputStream stream(serialized,false);stream.writeFloat(drive());stream.writeFloat(level());}
    void setStateInformation(const void* bytes,int size) override
    {if(size!=8)throw std::runtime_error("Invalid NAM amp control state.");juce::MemoryInputStream stream(bytes,(size_t)size,false);const auto driveValue=stream.readFloat(),levelValue=stream.readFloat();if(!controls(driveValue,levelValue))throw std::runtime_error("Invalid NAM amp controls.");}
    void processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi) override
    {
        juce::ScopedNoDenormals scoped;midi.clear();
        if(!rateOK.load()||audio.getNumChannels()!=2){audio.clear();return;}
        const auto values=published.read();const auto* routing=upstreamReference.load(std::memory_order_acquire);volume.setTargetValue(juce::Decibels::decibelsToGain(values.level));
        for(int pos=0;pos<audio.getNumSamples();pos+=chunk)
        {
            const int count=juce::jmin(chunk,audio.getNumSamples()-pos);
            gain.setTargetValue(juce::Decibels::decibelsToGain(values.drive)*captureInputGain*(routing&&!routing->at(pos)?1.f:values.calibration));
            for(int index=0;index<count;++index)input[(size_t)index]=audio.getSample(0,pos+index)*gain.getNextValue();
            float* in=input.data();float* out=output.data();
            try{dsp->process(&in,&out,count);}catch(...){audio.clear();fault.store(true);return;}
            for(int index=0;index<count;++index)
            {
                auto value=output[(size_t)index]*volume.getNextValue();if(!std::isfinite(value)){value=0;fault.store(true);}
                audio.setSample(0,pos+index,value);audio.setSample(1,pos+index,value);
            }
        }
    }
    std::atomic<bool> fault{false};
private:
    static_assert(std::atomic<float>::is_always_lock_free);
    static_assert(std::atomic<const tonegrid::calibration::ReferenceRouting*>::is_always_lock_free);
    std::unique_ptr<nam::DSP> dsp;
    const float captureInputGain;
    std::atomic<const tonegrid::calibration::ReferenceRouting*> upstreamReference{nullptr};bool invalidCalibration=false,prepared=false;
    std::atomic<float> calibrationGain{1.f};
    struct Controls{float drive=0,level=0,calibration=1.f;};tonegrid::ParameterMailbox<Controls> published;
    std::array<float,chunk> input{},output{};
    std::atomic<float> gainDb{0},volumeDb{0};std::atomic<bool> rateOK{true};
    juce::SmoothedValue<float> gain,volume;
};
class NamAmp final : public AmpSource
{
public:
    NamAmp(std::unique_ptr<nam::DSP> model,const namamp::State& state,std::optional<float> loudness={},bool lite=false,std::optional<tonegrid::calibration::Metadata> info={},float trimDb=0.f,bool hasTrim=false,float inputCompensationDb=0.f):identity(state.model),modelLoudness(loudness),modelNormalizationTrim(trimDb),measuredNormalization(hasTrim),modelInputCompensation(namamp::validatedInputCompensation(inputCompensationDb)),captureCalibration(info.value_or(tonegrid::calibration::metadata(*model))),pending(std::make_unique<NamAmpProcessor>(std::move(model),state,lite,modelInputCompensation)){pending->rejectCalibration(captureCalibration.invalid);}
    ~NamAmp() override{unload();}
    SourceType type() const noexcept override{return SourceType::Nam;}
    bool loaded() const noexcept override{return pending||node;}
    const juce::PluginDescription* pluginDescription() const noexcept override{return nullptr;}
    juce::AudioProcessorGraph::Node::Ptr addToGraph(juce::AudioProcessorGraph& graph) override
    {jassert(juce::MessageManager::existsAndIsCurrentThread());if(!pending||node)return nullptr;node=graph.addNode(std::move(pending));return node;}
    namamp::State state() const {auto* p=processor();return {identity,p?p->drive():0,p?p->level():0};}
    std::optional<float> loudnessDb() const noexcept{return modelLoudness;}
    float normalizationTrimDb() const noexcept{return modelNormalizationTrim;}
    bool hasNormalizationTrim() const noexcept{return measuredNormalization;}
    const tonegrid::calibration::Metadata& calibrationInfo() const noexcept{return captureCalibration;}
    float inputCompensationDb() const noexcept{return modelInputCompensation;}
    juce::String captureDescription() const
    {
        auto text=captureCalibration.description();
        if(modelInputCompensation!=0.f)text+="\nToneGrid input compensation: "+juce::String(modelInputCompensation>0?"+":"")+juce::String(modelInputCompensation,2)+" dB (listening adjustment, not electrical calibration).";
        return text;
    }
    void setUpstreamReference(const tonegrid::calibration::ReferenceRouting* value){if(auto* p=processor())p->setUpstreamReference(value);}
    void setCalibration(tonegrid::calibration::Settings value){if(auto* p=processor())p->setCalibration(value);}
    bool controls(float gain,float volume){return processor()&&processor()->controls(gain,volume);}
    bool supportsLite() const{return processor()&&processor()->supportsLite();}
    bool takeFault(){return processor()&&processor()->fault.exchange(false);}
    void captureState(juce::MemoryBlock& out) override{jassert(juce::MessageManager::existsAndIsCurrentThread());if(auto* p=processor())p->getStateInformation(out);}
    void restoreState(const juce::MemoryBlock& in) override{jassert(juce::MessageManager::existsAndIsCurrentThread());if(auto* p=processor())p->setStateInformation(in.getData(),(int)in.getSize());}
    juce::Result openEditor() override{return juce::Result::fail("NAM Amp has no plugin editor; use Gain and Volume in ToneGrid.");}
    bool editorVisible() const noexcept override{return false;}void closeEditor() override{}
    void unload() override{jassert(juce::MessageManager::existsAndIsCurrentThread());pending.reset();node=nullptr;}
private:
    NamAmpProcessor* processor() const{return pending?pending.get():node?static_cast<NamAmpProcessor*>(node->getProcessor()):nullptr;}
    const namamp::Identity identity;const std::optional<float> modelLoudness;const float modelNormalizationTrim;const bool measuredNormalization;const float modelInputCompensation;const tonegrid::calibration::Metadata captureCalibration;std::unique_ptr<NamAmpProcessor> pending;juce::AudioProcessorGraph::Node::Ptr node;
};
struct NamLoadResult {std::unique_ptr<AmpSource> source;juce::Result result=juce::Result::ok();};
inline NamLoadResult loadNamAmp(const namamp::Library& library,const namamp::State& state,bool lite=false)
{
    using namespace juce;
    if(!namamp::valid(state))return {nullptr,Result::fail("Invalid NAM amp identity/settings.")};
    try
    {
        namamp::Entry entry;nlohmann::json json;auto result=library.resolve(state.model,entry,json);if(result.failed())return {nullptr,result};
        // Optional loudness must not invalidate an otherwise valid network. The
        // core expects a numeric tag; drop only invalid loudness in this local
        // loading copy AFTER raw-file identity validation. Never rewrite the file.
        if(!entry.loudnessDb&&json.contains("metadata")&&json["metadata"].is_object())json["metadata"].erase("loudness");
        // Calibration is optional. Sanitize only this validated in-memory loading
        // copy, so a malformed optional tag cannot reject an otherwise valid NAM.
        tonegrid::calibration::sanitiseMetadata(json);
        nam::DspLoadOptions options;options.prewarm=false;auto model=nam::get_dsp(json,options);
        if(!model||model->NumInputChannels()!=1||model->NumOutputChannels()!=1||model->GetExpectedSampleRate()!=48000)return {nullptr,Result::fail("NAM amp must be mono and trained at 48000 Hz.")};
        auto resolved=state;resolved.model=entry.identity;
        return {std::make_unique<NamAmp>(std::move(model),resolved,entry.loudnessDb,lite,entry.calibration,entry.normalizationTrimDb,entry.hasNormalizationTrim,entry.inputCompensationDb),Result::ok()};
    }
    catch(const std::exception& error){return {nullptr,Result::fail("NAM amp load failed: "+String(error.what()))};}
    catch(...){return {nullptr,Result::fail("NAM amp load failed.")};}
}
}
