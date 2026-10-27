#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
namespace tonegrid::core {
struct IRenderProcessor {
 virtual void process(juce::AudioBuffer<float>&)=0;
 virtual bool realtimeOnly()const noexcept{return false;}
 virtual int latencySamples()const noexcept{return 0;}
 virtual ~IRenderProcessor()=default;
};
// Canonical PedalRig XML is opaque to Record. Only the application/Core handles it.
struct ToneService {
 std::function<juce::String()> capture;
 std::function<juce::Result(const juce::String&)> restore;
 std::function<std::unique_ptr<IRenderProcessor>(const juce::String&,double,juce::String&)> createRenderer;
 std::function<bool()> available;
 std::function<double()> deviceRate;
 std::function<int()> processingLatency;
};
}
