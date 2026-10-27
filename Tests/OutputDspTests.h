#pragma once
#include "Content/Presets/RigStore.h"
#include "OutputDspNodes.h"
#include "NamAmpTests.h"
#include "SelfTests.h"
namespace outputTests
{
inline bool run(juce::String& report)
{
    using namespace juce;using namespace tonegrid::output;
    bool ok=true;int checks=0;
    auto check=[&](bool good,const String& name){++checks;ok&=good;report+=String(good?"PASS ":"FAIL ")+name+"\n";};
    auto equal=[](const Settings& a,const Settings& b){const auto other=encode(b);return encode(a).isEquivalentTo(&other,false);};
    Settings controls;controls.ampNormalizationEnabled=false;controls.spreadEnabled=true;controls.offsetMs=-9.5f;controls.wobblePercent=37;controls.crossoverHz=260;controls.wobbleEnabled=false;controls.crossoverEnabled=false;controls.diffuseEnabled=false;
    Settings decoded;const auto xml=encode(controls);
    check(decode(xml,decoded)&&equal(decoded,controls),"Output DSP codec round-trips all independent controls");
    Settings mode;mode.ampNormalizationEnabled=true;mode.ampNormalizationEnhanced=false;
    check(decode(encode(mode),decoded)&&decoded.ampNormalizationEnabled&&!decoded.ampNormalizationEnhanced,"Partial normalization survives settings round-trip");
    mode.ampNormalizationEnhanced=true;auto legacyMode=encode(mode);legacyMode.removeAttribute("ampNormalizationMode");
    check(decode(legacyMode,decoded)&&decoded.ampNormalizationEnabled&&decoded.ampNormalizationEnhanced,"Legacy ON migrates to Full normalization");
    legacyMode.setAttribute("ampNormalizationEnabled",false);
    check(decode(legacyMode,decoded)&&!decoded.ampNormalizationEnabled,"Legacy OFF remains OFF");
    legacyMode.setAttribute("ampNormalizationMode","partial");check(!decode(legacyMode,decoded),"Conflicting normalization state rejects transactionally");
    legacyMode.setAttribute("ampNormalizationMode","invalid");check(!decode(legacyMode,decoded),"Unknown normalization mode rejects transactionally");
    bool reject=true;for(const auto* field:{"version","spreadEnabled","offsetMs","wobblePercent","crossoverHz"}){auto broken=xml;broken.removeAttribute(field);Settings kept=controls;reject&=!decode(broken,kept)&&equal(kept,controls);}
    check(reject,"Output DSP missing required fields reject transactionally");
    reject=true;for(const auto* text:{"nan","inf","12junk","25","-25"}){auto broken=xml;broken.setAttribute("offsetMs",text);Settings kept=controls;reject&=!decode(broken,kept)&&equal(kept,controls);}
    check(reject,"Output DSP malformed/nonfinite/out-of-range numbers reject without changing state");
    auto broken=xml;broken.setAttribute("diffuseEnabled","true");check(!decode(broken,decoded),"Output DSP invalid boolean is rejected rather than coerced");
    const auto temp=File::getSpecialLocation(File::tempDirectory),root=temp.getChildFile("ToneGrid-output-tests-"+Uuid().toString());
    ScopeGuard cleanup{[&]{if(root.isAChildOf(temp))root.deleteRecursively();}};
    const auto store=board::RigStore::forApplicationData(root);
    auto state=board::migrations::initialState();state.name="Output DSP synthetic";state.genericVst3=true;state.amp.append("opaque unchanged",16);
    PluginDescription description;description.name="Synthetic amp";description.pluginFormatName="VST3";description.fileOrIdentifier="synthetic.vst3";description.manufacturerName="Synthetic";description.uniqueId=123;description.version="test";
    board::RigIdentityLookup identity=[](int p,int preset){return "synthetic-"+String(p)+"-"+String(preset);};
    auto legacy=board::encode(state,description,identity);board::State loaded;PluginDescription required;
    check(legacy.getIntAttribute("version")==7&&!legacy.getChildByName("OutputDSP")&&board::decode(legacy,description,identity,loaded).wasOk()&&!loaded.outputDsp,"Legacy VST3 v7 encoding stays unchanged and new effects remain unspecified");
    check(!legacySettings().ampNormalizationEnabled&&!legacySettings().spreadEnabled,"Legacy rigs explicitly apply unity normalization and Spread bypass");
    state.outputDsp=controls;
    check(store.save(0,state,description,identity).result.wasOk()&&store.loadForSelection(0,identity,loaded,required).wasOk()&&loaded.outputDsp&&equal(*loaded.outputDsp,controls)&&loaded.amp==state.amp&&loaded.genericVst3,"VST3 v10 storage round-trip preserves controls and exact opaque amp bytes in temporary directory");
    const auto disk=XmlDocument::parse(store.slotFile(0));
    check(disk&&disk->getIntAttribute("version")==10&&vst3::Identity::of(required)==vst3::Identity::of(description),"New VST3 rig uses v10 with the existing stable plugin identity");
    auto corrupt=*disk;corrupt.removeChildElement(corrupt.getChildByName("OutputDSP"),true);auto preserved=loaded;
    check(board::decode(corrupt,description,identity,loaded).failed()&&loaded.amp==preserved.amp&&loaded.outputDsp&&equal(*loaded.outputDsp,*preserved.outputDsp),"V10 rig missing output DSP is rejected without publishing partial state");
    corrupt=*disk;corrupt.addChildElement(new XmlElement(xml));check(board::decode(corrupt,description,identity,loaded).failed(),"V10 duplicate output DSP is rejected");
    auto invalidState=state;invalidState.outputDsp->wobblePercent=101;const auto before=store.slotFile(0).loadFileAsString();
    check(store.save(0,invalidState,description,identity).result.failed()&&store.slotFile(0).loadFileAsString()==before,"Invalid output DSP save keeps existing rig untouched");
    namamp::Library library(namamp::libraryForApplicationData(root));namamp::Catalog catalog;library.refresh(catalog);
    auto model=nlohmann::json::parse(namTests::linear(.5f).toStdString());model["metadata"]={{"loudness",-24}};
    library.folder().getChildFile("Synthetic.nam").replaceWithText(String(model.dump()));library.refresh(catalog);
    namamp::State namState{catalog.entries[0].identity,0,0};auto source=amp::loadNamAmp(library,namState);
    check(source.source&&dynamic_cast<amp::NamAmp*>(source.source.get())->loudnessDb()==-24,"Real NAM file reader and facade publish loudness metadata outside inference engine");
    bool invalidMetadataLoads=true;
    for(const auto& invalid:std::vector<nlohmann::json>{"not-a-number",nullptr,42,-101})
    {
        model["metadata"]["loudness"]=invalid;const auto badFile=library.folder().getChildFile("InvalidOptionalLoudness.nam");badFile.replaceWithText(String(model.dump()));
        namamp::Entry entry;const auto originalHash=SHA256(badFile).toHexString();const auto read=library.read(badFile,entry);auto candidate=read.wasOk()?amp::loadNamAmp(library,{entry.identity,0,0}):amp::NamLoadResult{};
        const auto* facade=candidate.source?dynamic_cast<amp::NamAmp*>(candidate.source.get()):nullptr;
        invalidMetadataLoads&=read.wasOk()&&candidate.result.wasOk()&&facade&&!facade->loudnessDb()&&SHA256(badFile).toHexString()==originalHash&&entry.identity.sha256==originalHash;
    }
    check(invalidMetadataLoads,"Invalid optional loudness does not prevent valid NAM loading: unity metadata fallback, original file/hash untouched");
    state.sourceType=amp::SourceType::Nam;state.namAmp=namState;state.amp.reset();state.genericVst3=false;
    check(store.save(1,state,{},identity).result.wasOk()&&store.loadForSelection(1,identity,loaded,required).wasOk()&&loaded.sourceType==amp::SourceType::Nam&&loaded.namAmp.model==namState.model&&loaded.outputDsp&&equal(*loaded.outputDsp,controls),"NAM v10 storage keeps validated model identity plus independent output controls");
    state.outputDsp.reset();legacy=board::encode(state,{},identity);
    check(legacy.getIntAttribute("version")==8&&board::decode(legacy,{},identity,loaded).wasOk()&&!loaded.outputDsp,"Legacy native NAM v8 remains accepted without new output settings");
    // Noncommuting amp bias proves normalization occurs after amp, never before it.
    rig::StereoGraph graph;graph.setPlayConfigDetails(2,2,48000,128);using IO=AudioProcessorGraph::AudioGraphIOProcessor;
    auto input=graph.addNode(std::make_unique<IO>(IO::audioInputNode));auto pedal=graph.addNode(std::make_unique<TestEffect>(2,0));auto amp=graph.addNode(std::make_unique<TestEffect>(1,.1f));
    auto normalization=std::make_unique<NormalizationNode>();auto* norm=normalization.get();norm->configure(true,true,-24);
    auto normalize=graph.addNode(std::move(normalization));auto spreading=std::make_unique<SpreadNode>();auto* spread=spreading.get();
    Settings settings;settings.spreadEnabled=false;spread->configure(settings);auto wide=graph.addNode(std::move(spreading));auto output=graph.addNode(std::make_unique<IO>(IO::audioOutputNode));
    bool connected=rig::connectStereoChain(graph,input->nodeID,pedal->nodeID,amp->nodeID,normalize->nodeID);
    for(int channel=0;channel<2;++channel){connected&=graph.addConnection({{normalize->nodeID,channel},{wide->nodeID,channel}});connected&=graph.addConnection({{wide->nodeID,channel},{output->nodeID,channel}});}
    check(connected,"Stereo graph connects pedals -> amp -> separate normalization -> separate Spread -> output");
    graph.prepareToPlay(48000,128);AudioBuffer<float> audio(2,128);MidiBuffer midi;
    auto fill=[&]{for(int i=0;i<128;++i){audio.setSample(0,i,.1f);audio.setSample(1,i,-.15f);}};
    for(int block=0;block<12;++block){fill();graph.processBlock(audio,midi);}
    const float gain=Decibels::decibelsToGain(6.f);
    check(std::abs(audio.getSample(0,127)-.3f*gain)<1e-6&&std::abs(audio.getSample(1,127)+.2f*gain)<1e-6,"Graph renders noncommuting amp before normalization with no pedal normalization or dry mix");
    norm->configure(true,false,-24);fill();graph.processBlock(audio,midi);
    check(audio.getSample(0,127)==.3f&&std::abs(audio.getSample(1,127)+.2f)<1e-7,"Graph VST mode ignores loudness compensation and Spread bypass preserves L/R");
    settings.spreadEnabled=true;settings.wobbleEnabled=false;spread->configure(settings);
    for(int block=0;block<40;++block){fill();graph.processBlock(audio,midi);}
    spread->stereoOutputAvailable(false);fill();graph.processBlock(audio,midi);
    check(audio.getSample(0,127)==.3f&&std::abs(audio.getSample(1,127)+.2f)<1e-7,"Physical mono device suppresses Spread even when host graph has two processing channels");
    graph.releaseResources();
    report+=String(checks)+" output graph/storage checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
