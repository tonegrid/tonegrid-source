#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
namespace tonegrid::audio
{
// One stereo Lanczos boundary around the graph, as in TONE3000's ChainDomain.
// NAM stays at its trained rate; device taps, looper and recordings stay outside.
class DeviceRateProcessor final:public juce::AudioProcessor
{
public:
    static constexpr double engineRate=48000.;
    static bool supported(double rate) noexcept
    {return std::isfinite(rate)&&rate>=8000&&rate<=192000&&std::abs(rate-std::round(rate))<.01;}
    explicit DeviceRateProcessor(juce::AudioProcessor&);
    ~DeviceRateProcessor()override;
    const juce::String getName()const override{return "ToneGrid device rate";}
    bool acceptsMidi()const override{return false;}bool producesMidi()const override{return false;}
    bool hasEditor()const override{return false;}juce::AudioProcessorEditor* createEditor()override{return nullptr;}
    int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}
    const juce::String getProgramName(int)override{return {};}void changeProgramName(int,const juce::String&)override{}
    double getTailLengthSeconds()const override{return engine.getTailLengthSeconds();}
    void getStateInformation(juce::MemoryBlock&)override{}void setStateInformation(const void*,int)override{}
    bool isBusesLayoutSupported(const BusesLayout& l)const override
    {return l.getMainInputChannelSet()==juce::AudioChannelSet::stereo()&&l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();}
    bool converting()const noexcept;
    int conversionLatency()const noexcept{return delay;}
    void prepareToPlay(double,int)override;
    void releaseResources()override;
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&)override;
    std::atomic<bool> fault{false};
private:
    struct Conversion;
    std::unique_ptr<Conversion> conversion;
    juce::AudioProcessor& engine;
    juce::MidiBuffer emptyMidi;
    int slice=128,delay=0;bool valid=false;
};
}
