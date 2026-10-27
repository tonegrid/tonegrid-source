#pragma once
#include "TonexAmpSource.h"
// Diagnostic only: allow the real message loop to run between load/capture/restore.
class TonexInitializationProbe final:private juce::Timer
{
public:
    explicit TonexInitializationProbe(std::function<void(juce::String,bool)> done):complete(std::move(done))
    {
        auto loaded=amp::loadTonex(juce::File(amp::tonexDefaultPath()));source=std::move(loaded.source);
        if(!source){report="FAIL Load TONEX: "+loaded.result.getErrorMessage();startTimer(1);return;}
        graph.setPlayConfigDetails(2,2,48000,128);node=source->addToGraph(graph);plugin=dynamic_cast<juce::AudioPluginInstance*>(node->getProcessor());startTimer(3000);
    }
private:
    void timerCallback() override
    {
        using namespace juce;stopTimer();if(!source||!plugin){complete(report,false);return;}
        if(phase==0)
        {
            report="TONEX version="+source->pluginDescription()->version+" parameters="+String(plugin->getParameters().size())+"\n";
            source->captureState(state);for(auto* p:plugin->getParameters())values.push_back(p->getValue());
            for(int i=0;i<jmin(4,plugin->getParameters().size());++i)report+="PARAM "+String(i)+" "+plugin->getParameters()[i]->getName(128)+" = "+String(values[size_t(i)],8)+"\n";
            if(!values.empty())plugin->getParameters()[0]->setValueNotifyingHost(values[0]>.5f?.1f:.9f);
            source->restoreState(state);phase=1;startTimer(3000);return;
        }
        bool restored=true;for(int i=0;i<plugin->getParameters().size();++i){const auto value=plugin->getParameters()[i]->getValue();if(std::abs(value-values[size_t(i)])>=.0001f){restored=false;report+="DIFF "+plugin->getParameters()[i]->getName(128)+" expected="+String(values[size_t(i)],8)+" actual="+String(value,8)+"\n";}}
        plugin->prepareToPlay(48000,128);AudioBuffer<float> b(2,128);MidiBuffer midi;double energy=0;bool finite=true;
        for(int block=0;block<200;++block){for(int c=0;c<2;++c)for(int i=0;i<128;++i)b.setSample(c,i,.03f*(float)std::sin((block*128+i)*.071));plugin->processBlock(b,midi);for(int c=0;c<2;++c)for(int i=0;i<128;++i){const auto x=b.getSample(c,i);finite&=std::isfinite(x);energy+=x*x;}}
        plugin->releaseResources();report+="AFTER_MESSAGE_LOOP restored="+String(restored?1:0)+" finite="+String(finite?1:0)+" energy="+String(energy,9)+"\n";source->unload();graph.clear();complete(report,restored&&finite&&energy>1e-9);
    }
    std::function<void(juce::String,bool)> complete;std::unique_ptr<amp::AmpSource> source;rig::StereoGraph graph;juce::AudioProcessorGraph::Node::Ptr node;
    juce::AudioPluginInstance* plugin=nullptr;juce::String report;juce::MemoryBlock state;std::vector<float> values;int phase=0;
};
