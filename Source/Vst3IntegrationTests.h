#pragma once
#include "Vst3Discovery.h"
#include "Vst3Loader.h"
#include "Content/Presets/RigStore.h"

inline bool runVst3IntegrationTests(const juce::File& executable,juce::String& report)
{
    using namespace juce;bool ok=true;int checks=0;
    auto check=[&](bool value,const String& label){++checks;ok&=value;report+=String(value?"PASS ":"FAIL ")+label+"\n";};
    vst3::ScanProgress progress;auto scanned=vst3::scan(executable,vst3::defaultScanLocations(),progress);
    check(scanned.result.wasOk()&&!scanned.catalog.plugins.empty(),"Manual isolated discovery of installed VST3 plugins");
    for(const auto& d:scanned.catalog.plugins)report+="DISCOVERED "+vst3::label(d)+" | "+d.fileOrIdentifier+"\n";
    for(const auto& f:scanned.catalog.failures)report+="SCAN FAILURE "+f.location+" | "+f.reason+"\n";
    if(scanned.result.failed())return false;
    check(vst3::CacheStore(executable.getSiblingFile("vst3-test-cache.xml")).write(scanned.catalog),"Write generated integration cache artifact");
    const auto parent=File::getSpecialLocation(File::tempDirectory);
    const auto root=parent.getChildFile("ToneGrid-vst3-integration-"+Uuid().toString());
    const ScopeGuard cleanup{[&]{if(root.isAChildOf(parent))root.deleteRecursively();}};
    vst3::CacheStore cache(root.getChildFile("plugins.xml"));vst3::Catalog fromDisk;
    check(cache.write(scanned.catalog)&&cache.read(fromDisk).wasOk()&&fromDisk.plugins.size()==scanned.catalog.plugins.size(),"Real discovered cache survives new reader without rescan");
    auto exercise=[&](amp::LoadResult loaded,const PluginDescription& requested,int slot)
    {
        const auto name=requested.name;
        check(loaded.result.wasOk()&&loaded.source,"Load selected real VST3: "+name+" "+loaded.result.getErrorMessage());
        if(!loaded.source)return;
        auto& source=*loaded.source;const auto metadata=*source.pluginDescription();
        check(vst3::Identity::of(metadata)==vst3::Identity::of(requested),"Live metadata revalidates selected identity: "+name);
        rig::StereoGraph graph;graph.setPlayConfigDetails(2,2,48000,128);using IO=AudioProcessorGraph::AudioGraphIOProcessor;
        auto in=graph.addNode(std::make_unique<IO>(IO::audioInputNode)),out=graph.addNode(std::make_unique<IO>(IO::audioOutputNode));
        auto board=std::make_unique<board::Processor>();const auto result=board->load(tonegrid::paths::installed().models());
        check(result.wasOk(),"Load unchanged pedal engine for "+name);if(result.failed())return;
        auto* processor=board.get();auto pedals=graph.addNode(std::move(board));auto node=source.addToGraph(graph);
        auto* plugin=node?dynamic_cast<AudioPluginInstance*>(node->getProcessor()):nullptr;
        check(plugin&&rig::connectStereoChain(graph,in->nodeID,pedals->nodeID,node->nodeID,out->nodeID),"Stereo pedal chain -> selected VST3 -> output: "+name);
        if(!plugin)return;
        // Capture a NON-default automation value, then mutate again. This avoids
        // a fresh instance's defaults accidentally satisfying recreation tests.
        if(!plugin->getParameters().isEmpty())plugin->getParameters()[0]->setValueNotifyingHost(plugin->getParameters()[0]->isDiscrete()?1.f:.37f);
        MemoryBlock state;source.captureState(state);std::vector<float> values;
        for(auto* parameter:plugin->getParameters())values.push_back(parameter->getValue());
        auto sameValues=[&](AudioPluginInstance& instance)
        {bool same=instance.getParameters().size()==(int)values.size();for(int i=0;i<instance.getParameters().size()&&i<(int)values.size();++i)same&=std::abs(instance.getParameters()[i]->getValue()-values[(size_t)i])<.0001f;return same;};
        board::State snapshot;snapshot.genericVst3=true;snapshot.name="Synthetic "+name;snapshot.amp=state;snapshot.input=processor->front.get();
        for(int p=0;p<board::count;++p)snapshot.pedals[(size_t)p]=processor->get(p);
        board::RigIdentityLookup identities=[&](int p,int preset){return processor->identity(p,preset);};
        board::RigStore store(root.getChildFile("bank"),root.getChildFile("legacy"));board::State restored;PluginDescription required;
        const auto saved=store.save(slot,snapshot,metadata,identities);const auto read=store.loadForSelection(slot,identities,restored,required);
        check(saved.result.wasOk()&&read.wasOk()&&restored.genericVst3&&restored.amp==state,"Real VST3 v7 rig state byte-for-byte round trip: "+name);
        if(!values.empty())
        {
            auto* parameter=plugin->getParameters()[0];report+="MUTATED PARAMETER "+name+" : "+parameter->getName(128)+" automatable="+String(parameter->isAutomatable()?1:0)+" original="+String(values[0],8)+"\n";
            parameter->setValueNotifyingHost(values[0]>.5f?.1f:.9f);
        }
        source.restoreState(state);
        for(int index=0;index<plugin->getParameters().size()&&index<(int)values.size();++index)
            if(std::abs(plugin->getParameters()[index]->getValue()-values[(size_t)index])>=.0001f)
                report+="RESTORE DIFFERENCE "+name+" : "+plugin->getParameters()[index]->getName(128)+" expected="+String(values[(size_t)index],8)+" actual="+String(plugin->getParameters()[index]->getValue(),8)+"\n";
        check(sameValues(*plugin),"Real VST3 exposed parameters restore after mutation: "+name);
        if(plugin->hasEditor())
        {
            check(source.openEditor().wasOk()&&plugin->getActiveEditor()&&plugin->getActiveEditor()->getAudioProcessor()==plugin,"Open Amp opens selected native editor: "+name);
            auto* editor=plugin->getActiveEditor();auto* window=dynamic_cast<DocumentWindow*>(editor?editor->getTopLevelComponent():nullptr);
            if(window)window->closeButtonPressed();
            check(window&&!source.editorVisible()&&source.openEditor().wasOk()&&plugin->getActiveEditor()==editor,"Selected VST3 editor hides/reopens safely: "+name);
            source.closeEditor();
        }
        else report+="SKIP native editor: "+name+" has no GUI.\n";
        auto recreated=vst3::load(required,&state);
        {
            rig::StereoGraph second;auto next=recreated.source?recreated.source->addToGraph(second):nullptr;
            auto* instance=next?dynamic_cast<AudioPluginInstance*>(next->getProcessor()):nullptr;
            check(instance&&sameValues(*instance),"Real selected VST3 state survives full recreation: "+name);
            if(recreated.source)recreated.source->unload();second.clear();
        }
        graph.prepareToPlay(48000,128);AudioBuffer<float> audio(2,128);MidiBuffer midi;bool finite=true;double energy=0;
        for(int block=0;block<200;++block)
        {
            for(int c=0;c<2;++c)for(int i=0;i<128;++i)audio.setSample(c,i,.02f*(float)std::sin(2*MathConstants<double>::pi*110*(block*128+i)/48000));
            graph.processBlock(audio,midi);for(int c=0;c<2;++c)for(int i=0;i<128;++i){const float x=audio.getSample(c,i);finite&=std::isfinite(x);energy+=(double)x*x;}
        }
        report+="RENDER "+name+" finite="+String(finite?1:0)+" energy="+String(energy,12)+"\n";
        check(finite&&energy>1.e-9,"Real selected VST3 audio finite/non-silent in stereo: "+name);
        graph.releaseResources();source.unload();graph.clear();
        report+="PHYSICALLY TESTED "+name+" "+metadata.version+"\n";
    };
    bool tonex=false;
    for(const auto& d:fromDisk.plugins)if(d.name.containsIgnoreCase("TONEX")){exercise(vst3::load(d),d,0);tonex=true;break;}
    check(tonex,"Installed TONEX remains discovered/selectable");
    bool additional=false;
    for(const auto& preferred:{"Genome","BYOD","AmpliTube"})
    {
        for(const auto& d:fromDisk.plugins)if(!additional&&d.name.containsIgnoreCase(preferred))
        {
            auto loaded=vst3::load(d);
            if(loaded.result.failed()){report+="UNSUPPORTED/FAILED "+d.name+": "+loaded.result.getErrorMessage()+"\n";continue;}
            exercise(std::move(loaded),d,1);additional=true;break;
        }
        if(additional)break;
    }
    if(!additional)report+="SKIP additional plugin integration: no compatible optional installed VST3 available.\n";
    check(root.deleteRecursively()&&!root.exists(),"Remove temporary real-VST3 cache/rig storage");
    report+=String(checks)+" installed VST3 integration checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
