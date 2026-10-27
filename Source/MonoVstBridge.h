#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
namespace amp
{
// Keep ToneGrid's stereo graph while hosting a mono or mono-to-stereo effect.
// State and editor ownership stay with the original AudioPluginInstance.
class MonoVstBridge final:public juce::AudioProcessor
{
public:
    explicit MonoVstBridge(std::unique_ptr<juce::AudioPluginInstance> value)
        :AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),plugin(std::move(value)){}
    const juce::String getName()const override{return plugin->getName();}
    void prepareToPlay(double rate,int block)override{plugin->setRateAndBufferSizeDetails(rate,block);plugin->prepareToPlay(rate,block);setLatencySamples(plugin->getLatencySamples());}
    void releaseResources()override{plugin->releaseResources();}
    bool isBusesLayoutSupported(const BusesLayout& b)const override{return b.getMainInputChannelSet()==juce::AudioChannelSet::stereo()&&b.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();}
    void processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi)override
    {
        const int channels=juce::jmax(plugin->getTotalNumInputChannels(),plugin->getTotalNumOutputChannels());
        juce::AudioBuffer<float> view(audio.getArrayOfWritePointers(),channels,audio.getNumSamples());
        plugin->setPlayHead(getPlayHead());plugin->processBlock(view,midi);plugin->setPlayHead(nullptr);
        if(plugin->getTotalNumOutputChannels()==1)audio.copyFrom(1,0,audio,0,0,audio.getNumSamples());
    }
    double getTailLengthSeconds()const override{return plugin->getTailLengthSeconds();}
    bool acceptsMidi()const override{return plugin->acceptsMidi();}bool producesMidi()const override{return plugin->producesMidi();}
    bool hasEditor()const override{return false;}juce::AudioProcessorEditor* createEditor()override{return nullptr;}
    int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}
    const juce::String getProgramName(int)override{return {};}void changeProgramName(int,const juce::String&)override{}
    void getStateInformation(juce::MemoryBlock& b)override{plugin->getStateInformation(b);}void setStateInformation(const void* b,int n)override{plugin->setStateInformation(b,n);}
private:std::unique_ptr<juce::AudioPluginInstance> plugin;
};
}
