#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
// No driver or hardware is opened: drives the real AudioProcessorPlayer in tests.
class RawInputTestDevice final:public juce::AudioIODevice
{
public:
    RawInputTestDevice(double rate=48000,int block=256,int inputMask=1,int outputMask=3):AudioIODevice("Synthetic input","Test"),sr(rate),size(block),inMask(inputMask),outMask(outputMask){}
    juce::StringArray getOutputChannelNames() override{return outMask>3?juce::StringArray{"Out 1","Out 2","Headphones"}:juce::StringArray{"Left","Right"};}
    juce::StringArray getInputChannelNames() override{return inMask>1?juce::StringArray{"Input 1","Input 2"}:juce::StringArray{"Instrument"};}
    juce::Array<double> getAvailableSampleRates() override{return {sr};}
    juce::Array<int> getAvailableBufferSizes() override{return {size};}
    int getDefaultBufferSize() override{return size;}
    juce::String open(const juce::BigInteger&,const juce::BigInteger&,double,int) override{return {};}
    void close() override{}bool isOpen() override{return true;}
    void start(juce::AudioIODeviceCallback*) override{}void stop() override{}
    bool isPlaying() override{return true;}juce::String getLastError() override{return {};}
    int getCurrentBufferSizeSamples() override{return size;}double getCurrentSampleRate() override{return sr;}
    int getCurrentBitDepth() override{return 24;}
    juce::BigInteger getActiveOutputChannels() const override{return juce::BigInteger(outMask);}
    juce::BigInteger getActiveInputChannels() const override{return juce::BigInteger(inMask);}
    int getOutputLatencyInSamples() override{return 0;}int getInputLatencyInSamples() override{return 0;}
private:double sr;int size,inMask,outMask;
};
