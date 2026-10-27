#pragma once
#include "TonexAmpSource.h"
#include "Content/Presets/RigStore.h"

// Uses the installed TONEX and synthetic storage only. Never constructs RigHost,
// selects a hardware device or resolves the user's rig/preferences directories.
inline bool runAmpSourceIntegrationTests(const juce::File& models,
                                         const juce::PluginDescription& legacyDescription,
                                         const juce::PluginDescription& legacyInstanceDescription,
                                         const juce::MemoryBlock& legacyState,juce::String& report)
{
    using namespace juce;bool passed=true;int checks=0;
    auto check=[&](bool ok,const String& label){++checks;passed&=ok;report+=String(ok?"PASS ":"FAIL ")+label+"\n";};
    auto loaded=amp::loadTonex(File(amp::tonexDefaultPath()),48000,128);
    check(loaded.result.wasOk()&&loaded.source&&loaded.source->loaded(),"AmpSource factory loads installed TONEX: "+loaded.result.getErrorMessage());
    if(!loaded.source)return false;
    auto& source=*loaded.source;
    check(source.type()==amp::SourceType::Vst3,"Only active source is TONEX VST3");
    const auto* metadata=source.pluginDescription();
    check(metadata&&metadata->isDuplicateOf(legacyDescription)&&metadata->version==legacyDescription.version
          &&metadata->manufacturerName==legacyDescription.manufacturerName
          &&metadata->pluginFormatName==legacyDescription.pluginFormatName
          &&metadata->uniqueId==legacyDescription.uniqueId&&metadata->deprecatedUid==legacyDescription.deprecatedUid,
          "AmpSource preserves real TONEX identity accepted by legacy rig codec");
    source.restoreState(legacyState);
    rig::StereoGraph graph;graph.setPlayConfigDetails(2,2,48000,128);
    using IO=AudioProcessorGraph::AudioGraphIOProcessor;
    auto input=graph.addNode(std::make_unique<IO>(IO::audioInputNode));
    auto output=graph.addNode(std::make_unique<IO>(IO::audioOutputNode));
    auto board=std::make_unique<board::Processor>();const auto result=board->load(models);
    check(result.wasOk(),"AmpSource integration loads unchanged thirteen-pedal engine");
    if(result.failed())return false;
    auto* pedalsProcessor=board.get();auto pedals=graph.addNode(std::move(board));
    auto ampNode=source.addToGraph(graph);
    auto* plugin=ampNode?dynamic_cast<AudioPluginInstance*>(ampNode->getProcessor()):nullptr;
    // JUCE reports the inner binary's path for a live VST3 bundle instance,
    // whereas findAllTypesForFile describes the outer bundle. Compare each
    // description with its pre-boundary counterpart, not with one another.
    check(plugin&&plugin->getPluginDescription().isDuplicateOf(legacyInstanceDescription),
          "AmpSource graph uses native TONEX processor with unchanged live-instance identity");
    if(!plugin)return false;
    check(plugin->getTotalNumInputChannels()==2&&plugin->getTotalNumOutputChannels()==2,
          "AmpSource retains TONEX two-channel bus contract");
    check(rig::connectStereoChain(graph,input->nodeID,pedals->nodeID,ampNode->nodeID,output->nodeID),
          "AmpSource graph keeps TONEX after all thirteen pedals and before output");
    MemoryBlock state;source.captureState(state);
    check(!state.isEmpty(),"AmpSource captures real opaque TONEX state after graph transfer");
    std::vector<float> values;for(auto* parameter:plugin->getParameters())values.push_back(parameter->getValue());
    const auto valuesRestored=[&](AudioPluginInstance& instance)
    {
        bool same=instance.getParameters().size()==(int)values.size();
        for(int i=0;i<instance.getParameters().size()&&i<(int)values.size();++i)
            same&=std::abs(instance.getParameters()[i]->getValue()-values[(size_t)i])<.0001f;
        return same;
    };
    check(source.openEditor().wasOk()&&source.editorVisible()&&plugin->getActiveEditor(),
          "Open Amp opens installed TONEX editor through AmpSource");
    auto* editor=plugin->getActiveEditor();
    auto* window=dynamic_cast<DocumentWindow*>(editor?editor->getTopLevelComponent():nullptr);
    check(editor && editor->getAudioProcessor()==plugin && window && window->getName()=="TONEX - Amp",
          "Open Amp still uses same native TONEX editor, processor and window title");
    if(window)window->closeButtonPressed();
    check(!source.editorVisible()&&source.openEditor().wasOk()&&plugin->getActiveEditor()==editor,
          "Real TONEX editor close/reopen retains existing editor");
    source.closeEditor();
    check(plugin->getActiveEditor()==nullptr,"Real TONEX editor destroyed before rig restore");
    if(!values.empty())plugin->getParameters()[0]->setValueNotifyingHost(values[0]>.5f?.1f:.9f);
    source.restoreState(state);
    check(valuesRestored(*plugin),"AmpSource restores all real TONEX parameters after mutation");
    {
        const auto parent=File::getSpecialLocation(File::tempDirectory);
        const auto root=parent.getChildFile("ToneGrid-amp-source-"+Uuid().toString());
        const ScopeGuard cleanup{[&]{if(root.isAChildOf(parent))root.deleteRecursively();}};
        board::RigStore store(root.getChildFile("bank"),root.getChildFile("legacy"));
        const board::RigIdentityLookup identities=[&](int p,int preset){return pedalsProcessor->identity(p,preset);};
        board::State snapshot;snapshot.name="Synthetic AmpSource rig";snapshot.amp=state;snapshot.input=pedalsProcessor->front.get();
        for(int p=0;p<board::count;++p)snapshot.pedals[(size_t)p]=pedalsProcessor->get(p);
        const auto saved=store.save(0,snapshot,*metadata,identities);board::State restored;
        const auto recalled=store.load(0,*metadata,identities,restored);
        check(saved.result.wasOk()&&saved.nameIndexSaved&&recalled.wasOk()&&restored.amp==state,
              "AmpSource blob survives unchanged v6 RigStore codec byte-for-byte in temporary directory");
        if(recalled.wasOk())source.restoreState(restored.amp);
        check(recalled.wasOk()&&valuesRestored(*plugin),"Stored AmpSource rig restores real TONEX parameters");
        check(root.deleteRecursively()&&!root.exists(),"AmpSource temporary storage removed");
    }
    graph.prepareToPlay(48000,128);AudioBuffer<float> buffer(2,128);MidiBuffer midi;bool finite=true;double power=0;
    for(int block=0;block<200;++block)
    {
        for(int c=0;c<2;++c)for(int i=0;i<128;++i)buffer.setSample(c,i,.03f*(float)std::sin(2*MathConstants<double>::pi*110*(block*128+i)/48000));
        graph.processBlock(buffer,midi);
        for(int c=0;c<2;++c)for(int i=0;i<128;++i){const auto value=buffer.getSample(c,i);finite&=std::isfinite(value);power+=(double)value*value;}
    }
    check(finite&&power>1.e-9,"Real AmpSource path produces finite non-silent stereo audio");
    graph.releaseResources();source.unload();graph.clear();
    check(!source.loaded()&&!source.editorVisible(),"Real AmpSource unload leaves no live source/editor handle");
    {
        auto recreated=amp::loadTonex(File(amp::tonexDefaultPath()),48000,128);
        rig::StereoGraph freshGraph;auto node=recreated.source?recreated.source->addToGraph(freshGraph):nullptr;
        auto* instance=node?dynamic_cast<AudioPluginInstance*>(node->getProcessor()):nullptr;
        if(recreated.source)recreated.source->restoreState(state);
        check(instance&&valuesRestored(*instance),"TONEX state survives complete AmpSource recreation");
        if(recreated.source)recreated.source->unload();freshGraph.clear();
    }
    report+=String(checks)+" real TONEX amp-source checks; "+(passed?"ALL PASSED":"FAILURES")+"\n";
    return passed;
}
