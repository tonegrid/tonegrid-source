#pragma once
#include "Vst3Discovery.h"
#include "Vst3Loader.h"
#include "AmpReplacement.h"
#include "AmpSourceTests.h"
#include "Content/Presets/RigStore.h"
#include <future>

namespace vstTests
{
using namespace juce;
class Parameter final : public HostedAudioProcessorParameter
{
public:
    explicit Parameter(String identity,bool automatic=true):stableId(std::move(identity)),automatable(automatic){}
    String getParameterID() const override{return stableId;}
    float getValue() const override{return value;}
    void setValue(float next) override{value=next;}
    float getDefaultValue() const override{return 0;}
    String getName(int) const override{return "Synthetic macro";}
    String getLabel() const override{return {};}
    float getValueForText(const String& text) const override{return text.getFloatValue();}
    bool isAutomatable() const override{return automatable;}
    float value=0;
private:String stableId;bool automatable;
};
inline PluginDescription description(const File& file,int identifier=123)
{
    PluginDescription d;d.name="Same display name";d.manufacturerName="Synthetic vendor";d.pluginFormatName="VST3";
    d.uniqueId=identifier;d.deprecatedUid=identifier+1;d.version="1.0";d.fileOrIdentifier=file.getFullPathName();return d;
}
// Offline child emulates scan success, worker failure and a hung plugin without
// loading third-party binaries. Parent uses the production scan coordinator.
inline int fakeWorker(const File& input,const File& output)
{
    if(input.getFileName().startsWith("fail"))return 23;
    if(input.getFileName().startsWith("hang")){Thread::sleep(5000);return 24;}
    vst3::Catalog result;result.add(description(input));return vst3::CacheStore(output).write(result)?0:1;
}
inline bool run(String& report)
{
    bool ok=true;int checks=0;
    auto check=[&](bool value,const String& text){++checks;ok&=value;report+=String(value?"PASS ":"FAIL ")+text+"\n";};
    const auto parent=File::getSpecialLocation(File::tempDirectory);
    const auto root=parent.getChildFile("ToneGrid-vst3-tests-"+Uuid().toString());
    const ScopeGuard cleanup{[&]{if(root.isAChildOf(parent))root.deleteRecursively();}};
    check(root.createDirectory().wasOk(),"Prepare isolated VST3 test directories");
    {
        vst3::State original;original.opaque.append("raw",3);original.parameters={{"macro-17",.37f},{"macro-2",.8f}};vst3::State parsed;
        auto encoded=vst3::encodeState(original);
        check(vst3::decodeState(encoded,parsed)&&parsed.opaque==original.opaque&&parsed.parameters.size()==2&&parsed.parameters[0].value==.37f,
              "Extended VST3 state round trips opaque bytes and stable-ID automation values");
        parsed.opaque.reset();auto truncated=encoded;truncated.setSize(truncated.getSize()-1);
        check(!vst3::decodeState(truncated,parsed)&&parsed.opaque.isEmpty(),"Truncated extended state rejected transactionally");
        original.parameters[1].id="macro-17";check(!vst3::decodeState(vst3::encodeState(original),parsed),"Duplicate parameter IDs rejected");
        original.parameters[1].id="macro-2";original.parameters[0].value=std::numeric_limits<float>::quiet_NaN();
        check(!vst3::decodeState(vst3::encodeState(original),parsed),"Nonfinite automation value rejected");
        original.parameters[0].value=1.2f;check(!vst3::decodeState(vst3::encodeState(original),parsed),"Out-of-range automation value rejected");
        original.parameters[0].value=.37f;auto trailing=vst3::encodeState(original);trailing.append("x",1);
        check(!vst3::decodeState(trailing,parsed),"Trailing extended-state data rejected");
        ampTests::Trace stats;auto plugin=std::make_unique<ampTests::Plugin>(stats);
        auto first=std::make_unique<Parameter>("17"),secondParameter=std::make_unique<Parameter>("2"),meter=std::make_unique<Parameter>("meter",false);
        auto* macro=first.get();auto* secondMacro=secondParameter.get();auto* readOnly=meter.get();macro->value=.37f;secondMacro->value=.8f;
        plugin->addHostedParameter(std::move(first));plugin->addHostedParameter(std::move(secondParameter));plugin->addHostedParameter(std::move(meter));
        const auto metadata=plugin->getPluginDescription();amp::Vst3AmpHost source(nullptr,metadata,std::move(plugin),"Synthetic",true);MemoryBlock envelope;
        source.captureState(envelope);macro->value=.9f;secondMacro->value=.1f;readOnly->value=.75f;source.restoreState(envelope);
        check(macro->value==.37f&&secondMacro->value==.8f&&readOnly->value==.75f,"Restore supplements a plugin blob that omits automation, excluding non-automatable parameters");
        vst3::State unknown;vst3::decodeState(envelope,unknown);unknown.parameters[0].id="missing-ID";bool threw=false;
        try{source.restoreState(vst3::encodeState(unknown));}catch(...){threw=true;}
        check(threw,"Missing stable parameter ID reports restore failure rather than using parameter position");
    }
    const auto path=root.getChildFile("old.vst3"),movedPath=root.getChildFile("moved.vst3");
    auto a=description(path),b=description(root.getChildFile("other.vst3"),789);
    const auto identity=vst3::Identity::of(a);vst3::Identity restored;
    check(vst3::Identity::read(identity.xml(),restored)&&restored==identity,"Stable VST3 identity XML round trip");
    auto renamed=a;renamed.name="Renamed";renamed.fileOrIdentifier=movedPath.getFullPathName();renamed.version="2";
    check(vst3::Identity::of(renamed)==identity,"Identity independent of display name, location and version");
    check(!(vst3::Identity::of(b)==identity),"Duplicate display names retain distinct class identity");
    auto vendor=a;vendor.manufacturerName="Different vendor";check(!(vst3::Identity::of(vendor)==identity),"Manufacturer qualifies plugin identity");
    auto wrong=identity.xml();wrong.setAttribute("type","NAM");restored=identity;
    check(!vst3::Identity::read(wrong,restored)&&restored==identity,"Unsupported source identity rejected without mutation");
    vst3::Catalog list;list.add(a);list.add(b);list.add(a);list.sort();
    check(list.plugins.size()==2,"Catalog deduplicates same class/location but retains duplicate names");
    vst3::CacheStore cache(root.getChildFile("cache/plugins.xml"));vst3::Catalog read;
    check(cache.write(list)&&cache.read(read).wasOk()&&read.plugins.size()==2,"Cache persists full discovered metadata across fresh store");
    check(read.plugins[0].createXml()->isEquivalentTo(list.plugins[0].createXml().get(),false),"Cache preserves complete JUCE plugin description");
    PluginDescription resolved;resolved.name="sentinel";
    check(read.resolve(a,resolved).failed()&&resolved.name=="sentinel","Removed cached plugin reports missing without substitution or output mutation");
    // A bundle without its native binary is portable and safely rejected.
    // Never ask the OS loader to execute a text file disguised as a library.
    check(movedPath.createDirectory().wasOk(),"Create moved synthetic plugin bundle");
    auto moved=a;moved.fileOrIdentifier=movedPath.getFullPathName();vst3::Catalog rebuilt;rebuilt.add(moved);
    check(cache.write(rebuilt)&&cache.read(read).wasOk()&&read.plugins.size()==1,"Rescan rebuild replaces stale cache entries");
    check(read.resolve(a,resolved).wasOk()&&resolved.fileOrIdentifier==moved.fileOrIdentifier,"Moved identity resolves after cache rebuild");
    check(path.replaceWithText("synthetic")&&read.resolve(a,resolved).wasOk()&&resolved.fileOrIdentifier==a.fileOrIdentifier,"Existing location is only a resolution hint, preserving original candidate");
    path.deleteFile();const auto second=root.getChildFile("second.vst3");second.replaceWithText("synthetic");
    auto duplicate=moved;duplicate.fileOrIdentifier=second.getFullPathName();read.add(duplicate);
    check(read.resolve(a,resolved).failed(),"Ambiguous moved identity is rejected rather than silently choosing another copy");
    const auto priorCount=read.plugins.size();check(cache.file().replaceWithText("invalid XML")&&cache.read(read).failed()&&read.plugins.size()==priorCount,"Corrupt cache leaves published catalog intact");
    auto invalid=list.xml();invalid.getFirstChildElement()->setAttribute("format","AU");
    check(vst3::Catalog::decode(invalid,read).failed()&&read.plugins.size()==priorCount,"Non-VST3 cache metadata rejected transactionally");
    check(vst3::load(a).result.failed(),"Live loader rejects removed cached plugin");
    auto stale=description(movedPath);stale.uniqueId=777;
    check(vst3::load(stale).result.failed(),"Live loader revalidates cached file rather than trusting class identity");

    board::State state=board::migrations::initialState();state.name="Synthetic v7";state.genericVst3=true;state.amp.append("opaque",6);
    board::RigIdentityLookup models=[](int p,int preset){return "synthetic-"+String(p)+"-"+String(preset);};
    const auto xml=board::encode(state,a,models);board::State decoded;
    check(xml.getIntAttribute("version")==7&&xml.getChildByName("AmpSource")!=nullptr,"Generic rigs explicitly use v7 with typed VST3 identity");
    check(board::decode(xml,a,models,decoded).wasOk()&&decoded.genericVst3&&decoded.amp==state.amp,"Generic rig state/blob round trip");
    check(board::decode(xml,moved,models,decoded).wasOk(),"v7 accepts moved file with identical plugin identity/version");
    auto corruptState=state;vst3::State inner;inner.opaque.append("plugin",6);inner.parameters={{"bad",2.f}};corruptState.amp=vst3::encodeState(inner);
    check(board::decode(board::encode(corruptState,a,models),a,models,decoded).getErrorMessage()=="Invalid extended VST3 state.","v7 validates automation envelope before publishing/loading candidate");
    auto changed=a;changed.version="updated";decoded.name="sentinel";
    check(board::decode(xml,changed,models,decoded).wasOk(),"v7 accepts plugin updates with stable identity");
    decoded.name="sentinel";
    check(board::decode(xml,b,models,decoded).failed()&&decoded.name=="sentinel","Same display name does not authorize restoring another plugin");
    auto damaged=xml;damaged.getChildByName("Amp")->setAttribute("sha256","bad");
    check(board::decode(damaged,a,models,decoded).failed()&&decoded.name=="sentinel","v7 corrupt state rejected without partial publication");
    damaged=xml;damaged.getChildByName("AmpSource")->setAttribute("uniqueId",999);
    check(board::decode(damaged,a,models,decoded).failed(),"v7 typed identity must agree with preserved JUCE description");
    damaged=xml;damaged.addChildElement(new XmlElement(*xml.getChildByName("AmpSource")));
    check(board::decode(damaged,a,models,decoded).failed(),"v7 rejects duplicate typed amp identities");
    check(!xml.getChildByName("AmpSource")->hasAttribute("index")&&!xml.getChildByName("AmpSource")->hasAttribute("selectedId"),"Rig identity contains no dropdown position");
    board::RigStore store(root.getChildFile("rigs"),root.getChildFile("legacy"));
    check(store.save(0,state,a,models).result.wasOk(),"Save v7 plugin rig in synthetic bank");
    PluginDescription required;decoded.name="sentinel";
    check(store.loadForSelection(0,models,decoded,required).wasOk()&&vst3::Identity::of(required)==identity&&decoded.amp==state.amp,"Read and validate full candidate before loading selected plugin");
    vst3::Catalog absent;check(absent.resolve(required,resolved).failed()&&decoded.amp==state.amp,"Missing plugin leaves stored rig recoverable and never substitutes TONEX");
    check(store.load(0,moved,models,decoded).wasOk(),"Stored generic rig validates at resolved moved location");
    state.genericVst3=false;auto legacy=board::encode(state,a,models);
    check(legacy.getIntAttribute("version")==6&&!legacy.getChildByName("AmpSource"),"Legacy codec callers retain exact v6 writing/default behavior");
    check(board::decode(legacy,a,models,decoded).wasOk()&&!decoded.genericVst3,"v6 defaults to legacy TONEX semantics");
    check(board::decode(legacy,moved,models,decoded).failed(),"Legacy path/version identity remains strict");
    check(store.slotFile(1).replaceWithText("bad")&&store.loadForSelection(1,models,decoded,required).failed(),"Invalid rig rejected before plugin selection");

    const auto scanRoot=root.getChildFile("scan");scanRoot.createDirectory();
    for(const auto& name:{"good.vst3","fail.vst3","hang.vst3"})scanRoot.getChildFile(name).replaceWithText("synthetic worker input");
    vst3::ScanProgress progress;const auto worker=File::getSpecialLocation(File::currentExecutableFile);
    auto scanned=vst3::scan(worker,FileSearchPath(scanRoot.getFullPathName()),progress,1000);
    check(scanned.result.wasOk()&&scanned.catalog.plugins.size()==1&&scanned.catalog.failures.size()==2,"Isolated scan keeps success when another worker fails or times out");
    check(progress.completed.load()==3&&progress.total.load()==3,"Manual scan progress tracks all bundles");
    progress.cancel.store(true);scanned=vst3::scan(worker,FileSearchPath(scanRoot.getFullPathName()),progress,1000);
    check(scanned.result.failed(),"Cancelled scan cannot publish partial cache");
    const auto lateRoot=root.getChildFile("late-cancel");lateRoot.createDirectory();
    lateRoot.getChildFile("good.vst3").replaceWithText("fake");lateRoot.getChildFile("hang.vst3").replaceWithText("fake");
    progress.cancel.store(false);progress.completed.store(0);
    auto cancelling=std::async(std::launch::async,[&]{
        const auto start=Time::getMillisecondCounterHiRes();
        while(progress.completed.load()<1&&Time::getMillisecondCounterHiRes()-start<5000)Thread::sleep(5);
        Thread::sleep(100);progress.cancel.store(true);
    });
    scanned=vst3::scan(worker,FileSearchPath(lateRoot.getFullPathName()),progress,4000);cancelling.get();
    check(scanned.result.failed()&&scanned.catalog.plugins.size()==1,"Cancellation during last worker cannot publish an earlier successful partial cache");
    const auto failRoot=root.getChildFile("failed-scan");failRoot.createDirectory();failRoot.getChildFile("fail.vst3").replaceWithText("fake");progress.cancel.store(false);
    scanned=vst3::scan(worker,FileSearchPath(failRoot.getFullPathName()),progress,1000);
    check(scanned.result.failed(),"All workers failing prevents replacing prior cache with empty results");

    {
        ampTests::Trace trace;auto plugin=std::make_unique<ampTests::Plugin>(trace);auto* live=plugin.get();const auto metadata=plugin->getPluginDescription();
        amp::Vst3AmpHost source(nullptr,metadata,std::move(plugin),"Synthetic");MemoryBlock stateBlob;source.captureState(stateBlob);bool recoveryFailed=true;
        live->restoreFailuresRemaining=1;
        check(amp::restoreWithRollback(source,stateBlob,recoveryFailed).failed()&&!recoveryFailed&&live->gain==3,"Caught partial plugin restore rolls back previous blob");
        live->restoreFailuresRemaining=2;
        check(amp::restoreWithRollback(source,stateBlob,recoveryFailed).failed()&&recoveryFailed,"Failed rollback requests audio remain paused");
    }

    ampTests::Trace originalTrace,failedTrace,replacementTrace;
    {
        auto original=std::make_unique<ampTests::Plugin>(originalTrace);auto originalMetadata=original->getPluginDescription();
        auto* originalPointer=original.get();
        std::unique_ptr<amp::AmpSource> active=std::make_unique<amp::Vst3AmpHost>(nullptr,originalMetadata,std::move(original),"Synthetic");
        amp::Retirement retirement;rig::StereoGraph graph;graph.setPlayConfigDetails(2,2,48000,64);
        using IO=AudioProcessorGraph::AudioGraphIOProcessor;
        auto in=graph.addNode(std::make_unique<IO>(IO::audioInputNode)),out=graph.addNode(std::make_unique<IO>(IO::audioOutputNode));
        auto pedals=graph.addNode(std::make_unique<TestEffect>(2.f,1.f));auto node=active->addToGraph(graph);
        rig::connectStereoChain(graph,in->nodeID,pedals->nodeID,node->nodeID,out->nodeID);active->openEditor();
        auto broken=std::make_unique<ampTests::Plugin>(failedTrace,2,0);auto metadata=broken->getPluginDescription();
        auto candidate=std::make_unique<amp::Vst3AmpHost>(nullptr,metadata,std::move(broken),"Broken");
        const auto failed=amp::replace(graph,active,node,pedals->nodeID,out->nodeID,std::move(candidate),retirement);
        check(failed.failed()&&node->getProcessor()==originalPointer&&active->editorVisible()&&graph.getNodes().size()==4,"Failed graph replacement retains old instance, editor and serial graph");
        auto render=[&](float expected)
        {
            graph.prepareToPlay(48000,64);AudioBuffer<float> audio(2,64);MidiBuffer midi;
            for(int c=0;c<2;++c)for(int i=0;i<64;++i)audio.setSample(c,i,.25f);
            graph.processBlock(audio,midi);bool good=true;for(int c=0;c<2;++c)for(int i=0;i<64;++i)good&=std::abs(audio.getSample(c,i)-expected)<.0001f;
            graph.releaseResources();return good;
        };
        check(render(4.5f),"Failed replacement preserves previous native audio result");retirement.collect();
        auto next=std::make_unique<ampTests::Plugin>(replacementTrace);next->gain=.5f;metadata=next->getPluginDescription();
        const auto replaced=amp::replace(graph,active,node,pedals->nodeID,out->nodeID,std::make_unique<amp::Vst3AmpHost>(nullptr,metadata,std::move(next),"Next"),retirement);
        check(replaced.wasOk()&&graph.getNodes().size()==4&&!active->editorVisible()&&originalTrace.editorsDestroyed==1,"Successful replacement closes old editor and keeps amp separate downstream");
        check(render(.75f),"Successful replacement changes only amp transfer in stereo chain");retirement.collect();
        active->unload();graph.clear();
    }
    check(originalTrace.pluginsDestroyed==1&&originalTrace.pluginDestroyedAfterEditor&&failedTrace.pluginsDestroyed==1&&replacementTrace.pluginsDestroyed==1,
          "Replacement and retirement destroy every instance once after editors");
    check(root.deleteRecursively()&&!root.exists(),"Remove all synthetic VST3/cache/rig files");
    report+=String(checks)+" VST3 discovery/cache/selection checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
