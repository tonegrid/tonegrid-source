#pragma once
#include <juce_audio_devices/juce_audio_devices.h>

namespace audioRecovery126Tests
{
struct State{bool available=true,busy=true;int attempts=0,starts=0;};
class Device final:public juce::AudioIODevice
{
public:
    explicit Device(std::shared_ptr<State> value):AudioIODevice("Test Interface","Test ASIO"),state(std::move(value)){}
    ~Device()override{stop();}
    juce::StringArray getInputChannelNames()override{return {"Guitar","Input 2"};}
    juce::StringArray getOutputChannelNames()override{return {"Left","Right"};}
    juce::Array<double> getAvailableSampleRates()override{return {44100,48000,96000};}
    juce::Array<int> getAvailableBufferSizes()override{return {64,128,512};}
    int getDefaultBufferSize()override{return 128;}
    juce::String open(const juce::BigInteger& in,const juce::BigInteger& out,double sr,int block)override
    {
        ++state->attempts;stop();opened=false;error.clear();
        if(state->busy||!state->available){error="Test driver: interface is busy or disconnected";return error;}
        if(!getAvailableSampleRates().contains(sr)){error="Test driver: rejected sample rate";return error;}
        inputs=in;outputs=out;rate=sr;size=block;opened=true;return {};
    }
    void close()override{stop();opened=false;}
    bool isOpen()override{return opened;}
    void start(juce::AudioIODeviceCallback* value)override{if(!opened)return;callback=value;playing=true;++state->starts;if(callback)callback->audioDeviceAboutToStart(this);}
    void stop()override{if(playing&&callback)callback->audioDeviceStopped();playing=false;callback=nullptr;}
    bool isPlaying()override{return playing;}
    juce::String getLastError()override{return error;}
    int getCurrentBufferSizeSamples()override{return size;}
    double getCurrentSampleRate()override{return rate;}
    int getCurrentBitDepth()override{return 24;}
    juce::BigInteger getActiveInputChannels()const override{return inputs;}
    juce::BigInteger getActiveOutputChannels()const override{return outputs;}
    int getInputLatencyInSamples()override{return size;}
    int getOutputLatencyInSamples()override{return size;}
    bool render()
    {
        if(!callback)return false;juce::AudioBuffer<float> input(2,size),output(2,size);input.clear();output.clear();
        for(int c=0;c<2;++c)for(int i=0;i<size;++i)input.setSample(c,i,.1f);
        callback->audioDeviceIOCallbackWithContext(input.getArrayOfReadPointers(),2,output.getArrayOfWritePointers(),2,size,{});
        float peak=0;for(int c=0;c<2;++c)for(int i=0;i<size;++i){const auto sample=output.getSample(c,i);if(!std::isfinite(sample))return false;peak=juce::jmax(peak,std::abs(sample));}
        return peak>0;
    }
private:
    std::shared_ptr<State> state;juce::AudioIODeviceCallback* callback=nullptr;
    bool opened=false,playing=false;double rate=48000;int size=128;juce::BigInteger inputs,outputs;juce::String error;
};
class Type final:public juce::AudioIODeviceType
{
public:
    explicit Type(std::shared_ptr<State> value):AudioIODeviceType("Test ASIO"),state(std::move(value)){}
    void scanForDevices()override{}
    juce::StringArray getDeviceNames(bool)const override{return state->available?juce::StringArray{"Test Interface"}:juce::StringArray{};}
    int getDefaultDeviceIndex(bool)const override{return 0;}
    int getIndexOfDevice(juce::AudioIODevice*,bool)const override{return 0;}
    bool hasSeparateInputsAndOutputs()const override{return false;}
    juce::AudioIODevice* createDevice(const juce::String&,const juce::String&)override{return state->available?new Device(state):nullptr;}
private:std::shared_ptr<State> state;
};
}
