#pragma once
#include "CabinetState.h"
#include "Content/Cabinets/CabinetFolders.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_cryptography/juce_cryptography.h>

namespace tonegrid::cabinet
{
struct Prepared
{
    juce::AudioBuffer<float> samples;
    double rate=48000;
    State state;
    juce::Result result=juce::Result::ok();
};
// Decode, validate and normalize on the loading thread, never the audio callback.
inline Prepared prepare(const juce::File& requested,const juce::File& root,const State* expected=nullptr)
{
    using namespace juce;Prepared out;
    const auto fail=[&](const String& message){out.result=Result::fail(message);return std::move(out);};
    if(expected&&(!valid(*expected)||expected->file!=requested.getRelativePathFrom(root).replaceCharacter('\\','/')))return fail("Cabinet IR identity does not match the requested file.");
    const auto file=resolveCabinetFile(requested,root,expected);
    const auto relative=file.getRelativePathFrom(root).replaceCharacter('\\','/');
    if(!file.isAChildOf(root)||!safeRelative(relative)||!file.hasFileExtension("wav")||!file.existsAsFile()||file.getSize()>16*1024*1024)
        return fail("Invalid cabinet IR. Choose a WAV file up to 16 MiB.");
    for(auto p=file;p!=root;p=p.getParentDirectory())if(p.isSymbolicLink())return fail("Linked cabinet files are not supported.");
    out.state={relative,SHA256(file).toHexString(),expected?expected->locked:false,expected?expected->bypassed:false};
    if(expected&&(!valid(*expected)||expected->sha256!=out.state.sha256))return fail("Cabinet IR is missing or has changed. Previous sound kept.");
    auto stream=file.createInputStream();if(!stream)return fail("Cannot read this cabinet IR.");
    WavAudioFormat format;std::unique_ptr<AudioFormatReader> reader(format.createReaderFor(stream.release(),true));
    if(!reader||reader->sampleRate<8000||reader->sampleRate>192000||reader->numChannels<1||reader->numChannels>2
        ||reader->lengthInSamples<1||reader->lengthInSamples>2*reader->sampleRate)
        return fail("Use a mono or stereo cabinet WAV, 8–192 kHz, at most two seconds long.");
    out.rate=reader->sampleRate;out.samples.setSize(int(reader->numChannels),int(reader->lengthInSamples));
    if(!reader->read(&out.samples,0,out.samples.getNumSamples(),0,true,true))return fail("Cannot decode this cabinet IR.");
    double energy=0;
    for(int c=0;c<out.samples.getNumChannels();++c)for(int n=0;n<out.samples.getNumSamples();++n)
    {const auto v=out.samples.getSample(c,n);if(!std::isfinite(v))return fail("Cabinet IR contains invalid samples.");energy+=double(v)*v;}
    if(energy<1e-16)return fail("Cabinet IR is silent.");
    // Match mean response energy in the guitar band. Fixed gain only; no change
    // to NAM drive/calibration and no compressor in the audio path.
    int order=1;while((1<<order)<out.samples.getNumSamples())++order;
    dsp::FFT fft(order);const int size=1<<order;std::vector<float> spectrum(size_t(2*size),0.f);double band=0;int bins=0;
    for(int c=0;c<out.samples.getNumChannels();++c)
    {
        std::fill(spectrum.begin(),spectrum.end(),0.f);std::copy_n(out.samples.getReadPointer(c),out.samples.getNumSamples(),spectrum.data());
        fft.performRealOnlyForwardTransform(spectrum.data());
        for(int k=1;k<=size/2;++k)if(k*out.rate/size>=80&&k*out.rate/size<=8000)
        {band+=double(spectrum[size_t(2*k)])*spectrum[size_t(2*k)]+double(spectrum[size_t(2*k+1)])*spectrum[size_t(2*k+1)];++bins;}
    }
    if(bins==0||band<1e-16)return fail("Cabinet IR has no usable guitar-band response.");
    const float gain=float(jlimit(.063095734,3.981071706,1./std::sqrt(band/bins)));
    out.samples.applyGain(gain);return out;
}
class Node final:public juce::AudioProcessor
{
public:
    Node():AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)){}
    void load(Prepared&& ir)
    {
        jassert(juce::MessageManager::existsAndIsCurrentThread());
        convolution.loadImpulseResponse(std::move(ir.samples),ir.rate,juce::dsp::Convolution::Stereo::yes,
            juce::dsp::Convolution::Trim::no,juce::dsp::Convolution::Normalise::no);loaded=true;
    }
    void enable(bool value)noexcept{if(enabled.exchange(value,std::memory_order_acq_rel)!=value)resetRequested.store(true,std::memory_order_release);}
    const juce::String getName()const override{return "ToneGrid Cabinet";}
    void prepareToPlay(double rate,int block)override{convolution.prepare({rate,juce::uint32(juce::jmax(1,block)),2});}
    void releaseResources()override{convolution.reset();}
    void processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer&)override
    {
        juce::ScopedNoDenormals scope;
        if(resetRequested.exchange(false,std::memory_order_acq_rel))convolution.reset();
        if(!enabled.load(std::memory_order_acquire))return;
        if(!loaded){audio.clear();return;}
        juce::dsp::AudioBlock<float> block(audio);juce::dsp::ProcessContextReplacing<float> context(block);convolution.process(context);
    }
    bool isBusesLayoutSupported(const BusesLayout& l)const override{return l.getMainInputChannelSet()==juce::AudioChannelSet::stereo()&&l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();}
    bool acceptsMidi()const override{return false;}bool producesMidi()const override{return false;}
    bool hasEditor()const override{return false;}juce::AudioProcessorEditor* createEditor()override{return nullptr;}
    double getTailLengthSeconds()const override{return 2.;}
    int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}
    const juce::String getProgramName(int)override{return {};}void changeProgramName(int,const juce::String&)override{}
    void getStateInformation(juce::MemoryBlock&)override{}void setStateInformation(const void*,int)override{}
private:juce::dsp::Convolution convolution;std::atomic<bool> enabled{false},resetRequested{false};bool loaded=false;
};
}
