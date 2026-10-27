#pragma once
#include "Content/Presets/BoardState.h"
#include "Content/Presets/RigStore.h"
#include "AmpSourceIntegrationTests.h"
inline bool runIntegrationTests(const juce::File& models, juce::String& report)
{
    using namespace juce; bool passed=true;
    auto check=[&](bool ok,const String& text){report+=(ok?"PASS ":"FAIL ")+text+"\n";passed&=ok;};
    AudioPluginFormatManager formats;formats.addFormat(new VST3PluginFormat());OwnedArray<PluginDescription> types;
    formats.getFormat(0)->findAllTypesForFile(types,"C:\\Program Files\\Common Files\\VST3\\TONEX.vst3");
    if(types.isEmpty()){check(false,"TONEX installed and discoverable");return false;}const PluginDescription desc=*types[0];
    String error;
    auto create=[&](){return formats.createPluginInstance(desc,48000,128,error);};
    auto amp=create();check(amp!=nullptr,"Load one TONEX instance: "+error);if(!amp)return false;
    MemoryBlock state; amp->getStateInformation(state);check(!state.isEmpty(),"TONEX state captured");
    std::vector<float> values;for(auto* parameter:amp->getParameters())values.push_back(parameter->getValue());
    // Change a real hosted parameter, restore the blob, then recreate the plugin and restore again.
    if(!amp->getParameters().isEmpty())
    {
        auto* parameter=amp->getParameters()[0];float before=parameter->getValue();parameter->setValueNotifyingHost(before>.5f?.1f:.9f);
        amp->setStateInformation(state.getData(),(int)state.getSize());check(std::abs(parameter->getValue()-before)<.0001f,"TONEX parameter restored after changing it");
    }
    amp.reset();amp=create();if(!amp){check(false,"Recreate TONEX");return false;}
    amp->setStateInformation(state.getData(),(int)state.getSize());bool identical=amp->getParameters().size()==(int)values.size();
    for(int i=0;i<amp->getParameters().size()&&i<(int)values.size();++i)identical&=std::abs(amp->getParameters()[i]->getValue()-values[(size_t)i])<.0001f;
    check(identical,"All exposed TONEX parameter values restored after recreating instance");
    auto board=std::make_unique<board::Processor>();auto loaded=board->load(models);check(loaded.wasOk(),"Load curated board: "+loaded.getErrorMessage());if(loaded.failed())return false;
    // Milestone 2A: real TONEX bytes through the new storage boundary. Explicit
    // synthetic roots only; never construct RigHost or resolve user application data.
    {
        const auto tempParent=File::getSpecialLocation(File::tempDirectory);
        const auto tempRoot=tempParent.getChildFile("ToneGrid-tonex-storage-"+Uuid().toString());
        const ScopeGuard cleanup{[&]{if(tempRoot.isAChildOf(tempParent))tempRoot.deleteRecursively();}};
        check(tempRoot.createDirectory().wasOk(),"Create isolated TONEX storage test directory");
        const board::RigStore store(tempRoot.getChildFile("bank"),tempRoot.getChildFile("legacy"));
        const board::RigIdentityLookup identities=[&](int p,int preset){return board->identity(p,preset);};
        board::State snapshot;snapshot.name="Synthetic TONEX storage round trip";snapshot.amp=state;
        snapshot.input=board->front.get();for(int p=0;p<board::count;++p)snapshot.pedals[(size_t)p]=board->get(p);
        const auto saved=store.save(0,snapshot,desc,identities);
        check(saved.result.wasOk()&&saved.nameIndexSaved,"Save real TONEX blob via RigStore");
        board::State restored;const auto recalled=store.load(0,desc,identities,restored);
        check(recalled.wasOk()&&restored.amp==state&&restored.name==snapshot.name,
              "Load real TONEX blob byte-for-byte via RigStore");
        if(recalled.wasOk())
        {
            if(!amp->getParameters().isEmpty())amp->getParameters()[0]->setValueNotifyingHost(values[0]>.5f?.1f:.9f);
            amp->setStateInformation(restored.amp.getData(),(int)restored.amp.getSize());
            bool same=amp->getParameters().size()==(int)values.size();
            for(int i=0;i<amp->getParameters().size()&&i<(int)values.size();++i)
                same&=std::abs(amp->getParameters()[i]->getValue()-values[(size_t)i])<.0001f;
            check(same,"Real TONEX parameters restored from stored rig after mutation");
        }
        check(tempRoot.deleteRecursively()&&!tempRoot.exists(),"Remove isolated TONEX storage test directory");
    }
    rig::StereoGraph graph;graph.setPlayConfigDetails(2,2,48000,128);using IO=AudioProcessorGraph::AudioGraphIOProcessor;
    auto layout=amp->getBusesLayout();for(auto& b:layout.inputBuses)b=AudioChannelSet::disabled();for(auto& b:layout.outputBuses)b=AudioChannelSet::disabled();
    if(layout.inputBuses.isEmpty()||layout.outputBuses.isEmpty()){check(false,"TONEX buses");return false;}
    layout.inputBuses.getReference(0)=AudioChannelSet::stereo();layout.outputBuses.getReference(0)=AudioChannelSet::stereo();
    check(amp->setBusesLayout(layout),"TONEX accepts stereo graph layout");
    auto* processor=board.get();auto input=graph.addNode(std::make_unique<IO>(IO::audioInputNode));auto output=graph.addNode(std::make_unique<IO>(IO::audioOutputNode));
    auto pedals=graph.addNode(std::move(board));auto ampNode=graph.addNode(std::move(amp));
    check(rig::connectStereoChain(graph,input->nodeID,pedals->nodeID,ampNode->nodeID,output->nodeID),"Guitar -> thirteen pedals -> single TONEX -> stereo output");
    graph.prepareToPlay(48000,128);AudioBuffer<float> buffer(2,128);MidiBuffer midi;
    auto on=board::defaults(board::id(board::PedalId::TubeScreamer),0);on.enabled=true;processor->set(board::id(board::PedalId::TubeScreamer),on);
    bool finite=true;double energy=0;double start=Time::getMillisecondCounterHiRes();
    for(int b=0;b<1000;++b)
    {
        for(int j=0;j<128;++j){float x=.03f*(float)std::sin(2*MathConstants<double>::pi*110*(b*128+j)/48000);buffer.setSample(0,j,x);buffer.setSample(1,j,x);}
        graph.processBlock(buffer,midi);
        for(int c=0;c<2;++c)for(int j=0;j<128;++j){const auto x=buffer.getSample(c,j);finite&=std::isfinite(x);energy+=(double)x*x;}
    }
    check(finite&&energy>1.e-9,"Actual TONEX + TS9 processing generates finite non-silent stereo output");
    report+="TONEX + TS9 offline CPU fraction: "+String((Time::getMillisecondCounterHiRes()-start)/(128000.0/48.0),3)+"\n";
    on.enabled=false;processor->set(board::id(board::PedalId::TubeScreamer),on);
    for(int p=board::legacyPedalCount;p<board::count;++p)
    {
        auto setting=board::defaults(p,3);setting.enabled=true;processor->set(p,setting);bool good=true;double power=0;
        for(int b=0;b<200;++b){for(int j=0;j<128;++j){float x=.04f*(float)std::sin(2*MathConstants<double>::pi*220*(b*128+j)/48000);buffer.setSample(0,j,x);buffer.setSample(1,j,x);}graph.processBlock(buffer,midi);for(int c=0;c<2;++c)for(int j=0;j<128;++j){float x=buffer.getSample(c,j);good=good&&std::isfinite(x);if(b>20)power+=x*x;}}
        check(good&&power>1.e-9,"Actual "+board::pedalName(p)+" -> TONEX is finite and audible");setting.enabled=false;processor->set(p,setting);for(int b=0;b<12;++b){buffer.clear();graph.processBlock(buffer,midi);}
    }
    graph.releaseResources();graph.clear();
    const auto nativeDescription=static_cast<AudioPluginInstance*>(ampNode->getProcessor())->getPluginDescription();
    return runAmpSourceIntegrationTests(models,desc,nativeDescription,state,report)&&passed;
}
