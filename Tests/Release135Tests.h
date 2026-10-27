#pragma once
#include "Vst3AmpHost.h"
#include "Content/Presets/RigCodec.h"
#include "DeviceRateProcessor.h"
#include "RawInputTestDevice.h"
namespace release135Tests
{
class MonoPlugin final:public juce::AudioPluginInstance
{
public:
    explicit MonoPlugin(int outs):AudioPluginInstance(BusesProperties().withInput("Input",juce::AudioChannelSet::mono(),true).withOutput("Output",juce::AudioChannelSet::canonicalChannelSet(outs),true)),outputs(outs){}
    const juce::String getName()const override{return "Mono test";}
    void fillInPluginDescription(juce::PluginDescription& d)const override{d.name=getName();d.pluginFormatName="VST3";d.manufacturerName="Test";d.uniqueId=135;d.fileOrIdentifier="synthetic.vst3";d.version="1";}
    bool isBusesLayoutSupported(const BusesLayout& b)const override{return b.getMainInputChannelSet()==juce::AudioChannelSet::mono()&&b.getMainOutputChannelSet()==juce::AudioChannelSet::canonicalChannelSet(outputs);}
    void prepareToPlay(double,int)override{}void releaseResources()override{}
    void processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&)override{for(int i=0;i<b.getNumSamples();++i){const float x=b.getSample(0,i);b.setSample(0,i,2*x);if(outputs==2)b.setSample(1,i,3*x);}}
    double getTailLengthSeconds()const override{return 0;}bool acceptsMidi()const override{return false;}bool producesMidi()const override{return false;}
    bool hasEditor()const override{return false;}juce::AudioProcessorEditor* createEditor()override{return nullptr;}
    int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}
    const juce::String getProgramName(int)override{return {};}void changeProgramName(int,const juce::String&)override{}
    void getStateInformation(juce::MemoryBlock& b)override{b.replaceAll("state",5);}void setStateInformation(const void*,int)override{}
private:int outputs;
};
inline bool run(juce::String& report)
{
    using namespace juce;bool ok=true;int checks=0;const auto check=[&](bool value,const String& label){++checks;ok&=value;report+=(value?"PASS ":"FAIL ")+label+"\n";};
    for(int outs:{1,2})
    {
        auto plugin=std::make_unique<MonoPlugin>(outs);check(amp::Vst3AmpHost::configureStereo(*plugin,"Mono test").wasOk(),"mono input negotiation with "+String(outs)+" output(s)");
        auto description=plugin->getPluginDescription();amp::Vst3AmpHost source(nullptr,description,std::move(plugin),"Mono test",true);AudioProcessorGraph graph;
        auto node=source.addToGraph(graph);check(node&&node->getProcessor()->getTotalNumInputChannels()==2&&node->getProcessor()->getTotalNumOutputChannels()==2,"mono plugin is adapted to the stereo graph");
        if(!node)return false;auto* processor=node->getProcessor();processor->prepareToPlay(48000,128);AudioBuffer<float> audio(2,128);for(int i=0;i<128;++i){audio.setSample(0,i,.1f);audio.setSample(1,i,.4f);}MidiBuffer midi;processor->processBlock(audio,midi);
        check(std::abs(audio.getSample(0,127)-.2f)<1e-6&&std::abs(audio.getSample(1,127)-(outs==1?.2f:.3f))<1e-6,"mono output duplicates correctly; mono-to-stereo preserves both plugin outputs");
        MemoryBlock state;source.captureState(state);check(vst3::isEnvelope(state),"adapted plugin retains original VST state ownership");source.restoreState(state);processor->releaseResources();source.unload();graph.clear();
    }
    {
        rig::StereoGraph graph;graph.setPlayConfigDetails(2,2,48000,128);using IO=AudioProcessorGraph::AudioGraphIOProcessor;
        auto in=graph.addNode(std::make_unique<IO>(IO::audioInputNode));auto out=graph.addNode(std::make_unique<IO>(IO::audioOutputNode));
        for(int c=0;c<2;++c)graph.addConnection({{in->nodeID,c},{out->nodeID,c}});
        tonegrid::audio::DeviceRateProcessor boundary(graph);AudioProcessorPlayer player;RawInputTestDevice device(48000,128,2,4);
        player.audioDeviceAboutToStart(&device);player.setProcessor(&boundary);AudioBuffer<float> input(1,128),output(1,128);for(int i=0;i<128;++i)input.setSample(0,i,.125f);
        for(int block=0;block<5;++block)player.audioDeviceIOCallbackWithContext(input.getArrayOfReadPointers(),1,output.getArrayOfWritePointers(),1,128,{});
        check(device.getActiveInputChannels().findNextSetBit(0)==1&&device.getActiveOutputChannels().findNextSetBit(0)==2&&std::abs(output.getSample(0,127)-.125f)<1e-6&&!boundary.fault.load(),"second guitar input and a single non-default output remain audible through the actual stereo engine/player");
        player.setProcessor(nullptr);player.audioDeviceStopped();
    }
    report+=String(checks)+" Release135 mono/routing checks\n";return ok;
}
}
