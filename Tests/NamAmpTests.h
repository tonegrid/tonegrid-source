#pragma once
#include "NamAmp.h"
#include "Content/Presets/RigStore.h"
#include "AmpReplacement.h"
#include "AmpSourceTests.h"
#include <thread>

namespace namTests
{
using namespace juce;
inline String linear(float factor=1.f)
{return String(nlohmann::json{{"version","0.7.0"},{"architecture","Linear"},{"sample_rate",48000},{"config",{{"receptive_field",1},{"bias",false}}},{"weights",{factor}}}.dump());}
struct Trace {int resets=0,warms=0,processed=0,destroyed=0,maxFrames=0;bool heavyInCallback=false,inCallback=false;};
class Cubic final : public nam::DSP
{
public:
    explicit Cubic(Trace& counters):DSP(1,1,48000),trace(counters){}
    ~Cubic() override{++trace.destroyed;trace.heavyInCallback|=trace.inCallback;}
    void Reset(double,int) override{++trace.resets;trace.heavyInCallback|=trace.inCallback;}
    void prewarm() override{++trace.warms;trace.heavyInCallback|=trace.inCallback;}
    void process(float** input,float** output,int count) override
    {++trace.processed;trace.maxFrames=jmax(trace.maxFrames,count);for(int i=0;i<count;++i)output[0][i]=input[0][i]*input[0][i]*input[0][i];}
private:Trace& trace;
};
inline bool run(String& report)
{
    bool ok=true;int checks=0;auto check=[&](bool value,const String& text){++checks;ok&=value;report+=String(value?"PASS ":"FAIL ")+text+"\n";};
    const auto temp=File::getSpecialLocation(File::tempDirectory),root=temp.getChildFile("ToneGrid-nam-tests-"+Uuid().toString());
    ScopeGuard cleanup{[&]{if(root.isAChildOf(temp))root.deleteRecursively();}};
    namamp::Library library(namamp::libraryForApplicationData(root));namamp::Catalog catalog;
    check(library.refresh(catalog).wasOk()&&library.folder().isDirectory()&&catalog.entries.empty(),"NAM canonical folder auto-created under injected synthetic root; empty library valid");
    const auto a=library.folder().getChildFile("A.nam"),b=library.folder().getChildFile("B.NAM");
    a.replaceWithText(linear());b.replaceWithText(linear(.5f));library.folder().getChildFile("ignored.txt").replaceWithText(linear());
    const auto nested=library.folder().getChildFile("unrelated");nested.createDirectory();nested.getChildFile("not-scanned.nam").replaceWithText(linear());
    check(library.refresh(catalog).wasOk()&&catalog.entries.size()==3&&catalog.failures.empty(),"NAM discovery reads root and subfolders including uppercase extension");
    const auto identity=catalog.entries[0].identity;
    bool limitsRoundTrip=true;
    for(float drive:{-24.f,24.f})for(float volume:{-24.f,24.f})
    {namamp::State extremes{identity,drive,volume},restored;limitsRoundTrip&=namamp::decode(namamp::encode(extremes),restored)&&restored.gainDb==drive&&restored.volumeDb==volume;}
    check(limitsRoundTrip,"NAM Gain/Volume endpoints at -24 and +24 dB survive preset encoding and decoding");
    check(identity.file=="A.nam"&&identity.bytes==a.getSize()&&identity.sha256==SHA256(a).toHexString(),"NAM stable identity uses relative filename, size and SHA256, not dropdown index");
    namamp::Entry entry;nlohmann::json payload;check(library.resolve(identity,entry,payload).wasOk(),"NAM identity resolves unchanged file to validated JSON without model construction");
    auto wrong=identity;wrong.sha256=String::repeatedString("0",64);check(library.resolve(wrong,entry,payload).failed(),"NAM changed hash rejected before DSP load");
    wrong=identity;wrong.file="../A.nam";check(!wrong.valid()&&library.resolve(wrong,entry,payload).failed(),"NAM library traversal identity rejected");
    wrong=identity;wrong.file=a.getFullPathName();check(!wrong.valid(),"NAM absolute path cannot become persistent identity");
    const auto missing=library.folder().getChildFile("missing.nam");check(library.read(missing,entry).failed(),"NAM missing file reported without mutating catalog");
    const auto bad=library.folder().getChildFile("bad.nam");bad.replaceWithText("{corrupt");
    check(library.refresh(catalog).wasOk()&&catalog.entries.size()==3&&catalog.failures.size()==1,"Invalid NAM metadata is reported and excluded while valid models remain");
    auto json=nlohmann::json::parse(linear().toStdString());json["architecture"]="UnsupportedFutureNet";bad.replaceWithText(String(json.dump()));
    check(library.read(bad,entry).failed(),"NAM unknown architecture excluded without instantiation");
    json=nlohmann::json::parse(linear().toStdString());json["sample_rate"]=44100;bad.replaceWithText(String(json.dump()));check(library.read(bad,entry).failed(),"NAM incompatible training rate rejected; no silent resampling");
    json.erase("sample_rate");bad.replaceWithText(String(json.dump()));check(library.read(bad,entry).failed(),"NAM unknown training rate rejected explicitly");
    json=nlohmann::json::parse(linear().toStdString());json["version"]="9.0.0";bad.replaceWithText(String(json.dump()));check(library.read(bad,entry).failed(),"NAM unsupported file version rejected");
    json=nlohmann::json::parse(linear().toStdString());json.erase("weights");bad.replaceWithText(String(json.dump()));check(library.read(bad,entry).failed(),"NAM missing weights excluded during metadata pass");
    json=nlohmann::json::parse(linear().toStdString());json["weights"]=nlohmann::json::array();bad.replaceWithText(String(json.dump()));
    check(library.read(bad,entry).wasOk(),"NAM metadata eligibility does not claim full weight validation");
    const auto failed=amp::loadNamAmp(library,{entry.identity,0,0});check(failed.result.failed()&&!failed.source,"NAM malformed weights fail actual model construction safely");
    bad.deleteFile(); // Later refresh counts concern only A/B, not this eligible-but-unloadable fixture.
    auto loaded=amp::loadNamAmp(library,{identity,0,0});check(loaded.result.wasOk()&&loaded.source&&loaded.source->type()==amp::SourceType::Nam,"Valid NAM model loads as second explicit AmpSource implementation");
    if(loaded.source)
    {
        auto* facade=dynamic_cast<amp::NamAmp*>(loaded.source.get());
        check(facade&&facade->pluginDescription()==nullptr&&!facade->editorVisible()&&facade->openEditor().failed(),"NAM has typed identity and controls, no fabricated VST3 description/editor");
        rig::StereoGraph graph;auto node=loaded.source->addToGraph(graph);auto* processor=node?dynamic_cast<amp::NamAmpProcessor*>(node->getProcessor()):nullptr;
        check(processor&&processor->getTotalNumInputChannels()==2&&processor->getTotalNumOutputChannels()==2,"NAM graph contract is stereo while inference is mono");
        check(!loaded.source->addToGraph(graph),"NAM native processor can only transfer to graph once");
        if(processor)
        {
            processor->prepareToPlay(48000,513);AudioBuffer<float> buffer(2,513);MidiBuffer midi;
            for(int i=0;i<513;++i){buffer.setSample(0,i,.25f);buffer.setSample(1,i,-.75f);}processor->processBlock(buffer,midi);
            bool equal=true;for(int i=0;i<513;++i)equal&=buffer.getSample(0,i)==.25f&&buffer.getSample(1,i)==.25f;
            check(equal,"NAM Gain/Volume 0 dB strictly neutral to raw Linear model; channel zero duplicated, no averaging");
            processor->prepareToPlay(44100,128);buffer.clear();buffer.setSample(0,0,.5f);processor->processBlock(buffer,midi);
            check(buffer.getMagnitude(0,513)==0,"NAM processing refuses device sample rate other than 48000");
            processor->releaseResources();processor->prepareToPlay(48000,64);buffer.setSample(0,0,.5f);processor->processBlock(buffer,midi);check(buffer.getSample(0,0)==.5f,"NAM release/reprepare at 48000 remains usable");
            MemoryBlock controls;facade->captureState(controls);facade->controls(6,-9);facade->restoreState(controls);
            check(facade->state().gainDb==0&&facade->state().volumeDb==0,"NAM control-state round trip works without GUI");
            bool extremesRestore=true;
            for(float drive:{-24.f,24.f})for(float volume:{-24.f,24.f})
            {extremesRestore&=facade->controls(drive,volume);MemoryBlock saved;facade->captureState(saved);facade->controls(0,0);facade->restoreState(saved);extremesRestore&=facade->state().gainDb==drive&&facade->state().volumeDb==volume;}
            check(extremesRestore,"NAM processor control-state recall preserves both -24/+24 dB endpoints");facade->controls(0,0);
            check(!facade->controls(25,0)&&!facade->controls(-25,0)&&!facade->controls(0,25)&&!facade->controls(0,-25)&&!facade->controls(NAN,0)&&facade->state().gainDb==0,"NAM out-of-range and non-finite controls cannot mutate processor state");
        }
        loaded.source->unload();graph.clear();
    }
    {
        Trace trace;namamp::State state{identity,0,0};
        {
            amp::NamAmpProcessor processor(std::make_unique<Cubic>(trace),state);check(trace.resets==1&&trace.warms==1,"NAM Reset and prewarm performed before publication only");
            auto render=[&](int count){AudioBuffer<float> buffer(2,count);MidiBuffer midi;for(int i=0;i<count;++i)buffer.setSample(0,i,.5f);processor.processBlock(buffer,midi);return buffer.getSample(0,count-1);};
            processor.prepareToPlay(48000,4096);check(render(64)==.125f,"NAM neutral control output preserves nonlinear model response");
            processor.controls(6,0);const float first=render(1);check(first>.125f&&first<.13f,"NAM Gain change starts bounded 20 ms smoothing rather than jumping");
            const auto up=render(1920);check(std::abs(up-.125f*std::pow(Decibels::decibelsToGain(6.f),3))<.0001f,"Positive NAM Gain drives nonlinear inference before model, not a post gain");
            processor.controls(-6,0);check(std::abs(render(1920)-.125f*std::pow(Decibels::decibelsToGain(-6.f),3))<.0001f,"Negative NAM Gain reduces level before nonlinear model");
            processor.controls(0,-12);check(std::abs(render(1920)-.125f*Decibels::decibelsToGain(-12.f))<.0001f,"NAM Volume acts after inference with correct dB scaling");
            processor.controls(0,12);check(std::abs(render(1920)-.125f*Decibels::decibelsToGain(12.f))<.0001f,"NAM positive Volume follows documented +12 dB range");
            processor.controls(0,0);const float smoothingStart=render(1);check(smoothingStart>.125f&&smoothingStart<.5f,"NAM Volume changes are smoothed independently");
            std::thread audio([&]{trace.inCallback=true;render(2048);trace.inCallback=false;});audio.join();
            processor.releaseResources();processor.prepareToPlay(48000,1024);
            check(trace.resets==1&&trace.warms==1&&!trace.heavyInCallback&&trace.maxFrames<=128,"Audio processing/reprepare performs no model Reset/prewarm/destruction and uses bounded slices");
        }
        check(trace.destroyed==1&&!trace.heavyInCallback,"Heavy NAM model destroyed once outside simulated callback");
    }
    const board::RigIdentityLookup models=[](int p,int n){return "Synthetic pedal "+String(p)+"/"+String(n);};
    auto state=board::migrations::initialState();state.name="NAM rig";state.sourceType=amp::SourceType::Nam;state.namAmp={identity,5,-8};state.input.gainDb=2;state.pedals[0].enabled=true;
    const PluginDescription none;auto encoded=board::encode(state,none,models);board::State decoded;
    check(encoded.getIntAttribute("version")==8&&!encoded.getChildByName("PLUGIN")&&!encoded.getChildByName("Amp")&&encoded.getChildByName("AmpSource")->getStringAttribute("type")=="NAM","NAM rig v8 serializes explicit source type and no plugin/blob/dropdown identity");
    check(board::decode(encoded,none,models,decoded).wasOk()&&decoded.sourceType==amp::SourceType::Nam&&decoded.namAmp.model==identity&&decoded.namAmp.gainDb==5&&decoded.namAmp.volumeDb==-8&&decoded.input.gainDb==2&&decoded.pedals[0].enabled,"NAM rig complete typed snapshot round trip preserves controls and unrelated state");
    auto corrupt=encoded;corrupt.getChildByName("AmpSource")->setAttribute("gainDb",99);decoded.name="unchanged";
    check(board::decode(corrupt,none,models,decoded).failed()&&decoded.name=="unchanged","NAM invalid gain recall is transactional");
    corrupt=encoded;corrupt.getChildByName("AmpSource")->setAttribute("gainDb","invalid");check(board::decode(corrupt,none,models,decoded).failed(),"NAM malformed numeric control cannot be silently interpreted as neutral");
    corrupt=encoded;corrupt.getChildByName("AmpSource")->setAttribute("file","../outside.nam");check(board::decode(corrupt,none,models,decoded).failed(),"NAM rig outside-library path rejected");
    corrupt=encoded;corrupt.getChildByName("AmpSource")->setAttribute("type","0");check(board::decode(corrupt,none,models,decoded).failed(),"NAM persisted source type never interprets UI integer/display index");
    corrupt=encoded;corrupt.addChildElement(new XmlElement(*corrupt.getChildByName("AmpSource")));check(board::decode(corrupt,none,models,decoded).failed(),"NAM duplicate source rejected");
    corrupt=encoded;corrupt.addChildElement(new XmlElement("PLUGIN"));check(board::decode(corrupt,none,models,decoded).failed(),"NAM rig cannot smuggle conflicting VST3 description");
    const auto store=board::RigStore::forApplicationData(root);
    check(store.save(0,state,none,models).result.wasOk()&&store.readName(0)==state.name,"NAM rig filesystem save uses existing slot/name handling in synthetic root");
    PluginDescription required;check(store.loadForSelection(0,models,decoded,required).wasOk()&&decoded.sourceType==amp::SourceType::Nam&&required.name.isEmpty(),"NAM storage selection loader returns typed state without fabricating plugin metadata");
    const auto fileBefore=store.slotFile(0).loadFileAsString();a.deleteFile();auto absent=amp::loadNamAmp(library,decoded.namAmp);
    check(absent.result.failed()&&!absent.source&&store.slotFile(0).loadFileAsString()==fileBefore,"Missing NAM file leaves saved rig recoverable without rewriting or substitution");
    check(library.refresh(catalog).wasOk()&&catalog.entries.size()==2&&catalog.entries[0].identity.file=="B.NAM","NAM refresh removes deleted entry without fallback identity substitution");
    a.replaceWithText(linear(.25f));library.refresh(catalog);check(catalog.entries.size()==3&&!(catalog.entries[0].identity==identity)&&amp::loadNamAmp(library,{identity,0,0}).result.failed(),"NAM same filename with changed content rejected until explicit reselection");
    {
        ampTests::Trace vst;std::unique_ptr<amp::AmpSource> active;
        auto plugin=std::make_unique<ampTests::Plugin>(vst);const auto metadata=plugin->getPluginDescription();active=std::make_unique<amp::Vst3AmpHost>(nullptr,metadata,std::move(plugin),"Synthetic");
        amp::Retirement retired;rig::StereoGraph graph;graph.setPlayConfigDetails(2,2,48000,64);using IO=AudioProcessorGraph::AudioGraphIOProcessor;
        auto input=graph.addNode(std::make_unique<IO>(IO::audioInputNode)),output=graph.addNode(std::make_unique<IO>(IO::audioOutputNode)),pedals=graph.addNode(std::make_unique<TestEffect>(2,1));auto node=active->addToGraph(graph);
        rig::connectStereoChain(graph,input->nodeID,pedals->nodeID,node->nodeID,output->nodeID);
        auto previous=node->getProcessor();auto failedSwitch=amp::loadNamAmp(library,{identity,0,0});
        check(failedSwitch.result.failed()&&active->type()==amp::SourceType::Vst3&&node->getProcessor()==previous,"Failed NAM load preserves previous VST3 graph instance");
        auto next=amp::loadNamAmp(library,{catalog.entries[0].identity,0,0});
        check(next.source&&amp::replace(graph,active,node,pedals->nodeID,output->nodeID,std::move(next.source),retired).wasOk()&&active->type()==amp::SourceType::Nam,"VST3 to NAM uses common safe AmpSource replacement");
        graph.prepareToPlay(48000,64);AudioBuffer<float> buffer(2,64);buffer.clear();for(int c=0;c<2;++c)for(int i=0;i<64;++i)buffer.setSample(c,i,.25f);MidiBuffer midi;graph.processBlock(buffer,midi);
        check(std::abs(buffer.getSample(0,0)-.375f)<.0001f&&buffer.getSample(1,0)==buffer.getSample(0,0),"NAM is after noncommutative pedal stage, then stereo output; not pedal fourteen");graph.releaseResources();retired.collect();
        previous=node->getProcessor();failedSwitch=amp::loadNamAmp(library,{identity,0,0});check(failedSwitch.result.failed()&&node->getProcessor()==previous&&active->type()==amp::SourceType::Nam,"Failed NAM replacement preserves previous valid NAM model");
        next=amp::loadNamAmp(library,{catalog.entries[1].identity,0,0});check(next.source&&amp::replace(graph,active,node,pedals->nodeID,output->nodeID,std::move(next.source),retired).wasOk(),"Valid NAM model change replaces only amp node");
        auto back=std::make_unique<ampTests::Plugin>(vst);const auto backDescription=back->getPluginDescription();
        check(amp::replace(graph,active,node,pedals->nodeID,output->nodeID,std::make_unique<amp::Vst3AmpHost>(nullptr,backDescription,std::move(back),"Synthetic"),retired).wasOk()&&active->type()==amp::SourceType::Vst3,"NAM to VST3 retains existing native host and common downstream boundary");
        active->unload();graph.clear();
    }
    check(root.deleteRecursively()&&!root.exists(),"NAM synthetic library/storage cleanup leaves no user data touched");
    report+=String(checks)+" NAM amp checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
