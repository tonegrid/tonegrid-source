#pragma once
#include "ReverbTests.h"
#include "Effects/Reverb/SpringReverbProcessor.h"
#include "Reference/Reverb115Processor.h"
#include "Reference/OutputDsp115State.h"
namespace springTests
{
using namespace juce;
using namespace tonegrid::reverb;
inline bool run(String& report)
{
    using reverbTests::hash;using reverbTests::energy;using reverbTests::finite;
    bool ok=true;int checks=0;auto check=[&](bool value,const String& text){++checks;ok&=value;report+=String(value?"PASS ":"FAIL ")+text+"\n";};
    check(int(Type::Room)==0&&int(Type::Plate)==1&&int(Type::Large)==2&&int(Type::Spring)==3,"Spring enum appends ID 3 without renumbering Dattorro identities");
    Parameters p{true,Type::Spring,1.5f,0,40,100};
    tonegrid::SpringReverbProcessor core;
    check(!core.isPrepared()&&core.processSample(1,1)==std::array<float,2>{},"Unprepared spring wrapper is silent and safe");
    bool rejected=false;try{core.prepare(0,96);}catch(const std::invalid_argument&){rejected=true;}
    check(rejected&&!core.setParameters(NAN,40)&&!core.setParameters(11,40)&&!core.setParameters(1,-1),"Spring preparation/control invalid values rejected outside callback");
    core.prepare(48000,96);core.setParameters(1.5f,40);const auto mallocs=core.preparationAllocations();
    check(core.isPrepared()&&mallocs==6,"Spring owns six prepared C buffers/converters per instance");
    for(int i=0;i<10000;++i)core.processSample(.01f,.01f);core.reset();double silent=0;
    for(int i=0;i<5000;++i){auto out=core.processSample(0,0);silent+=out[0]*out[0]+out[1]*out[1];}
    check(silent==0&&core.preparationAllocations()==mallocs,"Spring reset clears tank/frame history without allocation or engine construction");
    auto normal=reverbTests::impulse(p);
    check(finite(normal)&&energy(normal)>1e-7,"Spring wet impulse remains finite and generates a tail");
    check(reverbTests::onset(normal)>tonegrid::SpringReverbProcessor::wetFrameDelaySamples,"Spring has natural dispersive onset plus fixed 32-sample wet framing delay");
    double width=0;for(int i=0;i<normal.getNumSamples();++i)width+=std::abs(normal.getSample(0,i)-normal.getSample(1,i));
    check(width>1e-5,"Four Spring lines generate a restrained stereo wet image");
    auto mono=reverbTests::impulse(p,1);bool collapsed=true;
    for(int i=0;i<mono.getNumSamples();++i)collapsed&=std::abs(mono.getSample(0,i)-(normal.getSample(0,i)+normal.getSample(1,i))*.5f)<1e-6;
    check(collapsed,"Spring mono input/output safely averages wet L/R");
    tonegrid::ReverbProcessor deviceMono;deviceMono.setTarget(p);deviceMono.prepare(48000,96);AudioBuffer<float> warm(2,4800);warm.clear();deviceMono.process(warm,false);
    AudioBuffer<float> physical(2,normal.getNumSamples());physical.clear();physical.setSample(0,0,1);physical.setSample(1,0,1);deviceMono.process(physical,false);bool deviceCollapse=true;
    for(int i=0;i<physical.getNumSamples();++i)deviceCollapse&=physical.getSample(0,i)==physical.getSample(1,i)&&std::abs(physical.getSample(0,i)-mono.getSample(0,i))<1e-6;
    check(deviceCollapse,"Spring physical mono output averages wet in the stereo host graph");
    tonegrid::ReverbProcessor partitioned;partitioned.setTarget(p);partitioned.prepare(48000,96);warm.clear();partitioned.process(warm);AudioBuffer<float> chunks(2,normal.getNumSamples());chunks.clear();int cursor=0,part=0;
    constexpr std::array<int,6> sizes{1,3,31,96,257,1000};
    while(cursor<chunks.getNumSamples()){int n=std::min(sizes[size_t(part++)%sizes.size()],chunks.getNumSamples()-cursor);AudioBuffer<float> piece(2,n);piece.clear();if(cursor==0){piece.setSample(0,0,1);piece.setSample(1,0,1);}partitioned.process(piece);for(int c=0;c<2;++c)chunks.copyFrom(c,cursor,piece,c,0,n);cursor+=n;}
    check(hash(chunks)==hash(normal),"Spring wet framing is bit-identical across irregular callback partitions");
    partitioned.reset();warm.clear();partitioned.process(warm);AudioBuffer<float> anti(2,1024);for(int i=0;i<1024;++i){anti.setSample(0,i,.1f*std::sin(i*.07f));anti.setSample(1,i,-anti.getSample(0,i));}partitioned.process(anti);
    check(energy(anti)==0,"Spring mono tank excitation explicitly cancels anti-phase stereo input at fully wet");
    for(auto from:{Type::Room,Type::Spring})for(auto to:{Type::Spring,Type::Plate,Type::Large})if(from!=to)
    {
        tonegrid::ReverbProcessor a,b;auto start=p;start.type=from;a.setTarget(start);b.setTarget(start);a.prepare(48000,96);b.prepare(48000,96);AudioBuffer<float> x(2,96),y(2,96);
        auto fill=[&](int block){for(int i=0;i<96;++i){x.setSample(0,i,.05f*std::sin((block*96+i)*.071f));x.setSample(1,i,.03f*std::cos((block*96+i)*.043f));}y.makeCopyOf(x);};
        for(int n=0;n<100;++n){fill(n);a.process(x);b.process(y);}auto finish=start;finish.type=to;b.setTarget(finish);fill(100);a.process(x);b.process(y);
        check(finite(y)&&std::abs(x.getSample(0,0)-y.getSample(0,0))<.001f,"Spring type handoff begins continuously: "+String(int(from))+" -> "+String(int(to)));
    }
    auto pre=p;pre.preDelayMs=100;auto delayed=reverbTests::impulse(pre);
    check(reverbTests::onset(delayed)-reverbTests::onset(normal)==4800,"Spring common 100 ms pre-delay shifts onset by exactly 4800 samples");
    auto dark=p;dark.color=0;auto bright=p;bright.color=100;auto darkIR=reverbTests::impulse(dark),brightIR=reverbTests::impulse(bright);
    check(finite(darkIR)&&finite(brightIR)&&hash(darkIR)!=hash(brightIR)&&reverbTests::highFrequencyRatio(brightIR)>reverbTests::highFrequencyRatio(darkIR),"Spring Color extremes remain finite and brighter tone increases spectral brightness");
    auto brief=p;brief.timeSeconds=.2f;auto longP=p;longP.timeSeconds=10;
    auto shortIR=reverbTests::impulse(brief),longIR=reverbTests::impulse(longP);
    check(finite(shortIR)&&finite(longIR)&&energy(longIR,48000)>energy(shortIR,48000)*2,"Spring Time bounds remain stable and longer decay retains more late energy");
    for(double rate:{8000.,44100.,96000.,192000.}){auto ir=reverbTests::impulse(p,2,rate);check(finite(ir)&&energy(ir)>1e-8,"Spring sample-rate preparation/render at "+String(rate));}
    tonegrid::ReverbProcessor dsp;dsp.setTarget(p);dsp.prepare(48000,96);AudioBuffer<float> audio(2,96);MidiBuffer midi;
    double peak=0;bool strongFinite=true;for(int block=0;block<1500;++block){audio.clear();if(block==0){audio.setSample(0,0,4);audio.setSample(1,0,4);}dsp.process(audio);strongFinite&=finite(audio);for(int c=0;c<2;++c)for(int i=0;i<96;++i)peak=std::max(peak,double(std::abs(audio.getSample(c,i))));}
    check(strongFinite&&std::isfinite(peak)&&peak<20,"Spring strong +12 dBFS transient remains finite with bounded wet output");
    auto ramp=p;ramp.enabled=false;dsp.setTarget(ramp);audio.clear();audio.setSample(0,0,.5f);audio.setSample(1,0,.5f);dsp.process(audio);
    check(audio.getSample(0,0)>0&&audio.getSample(0,0)<.5f,"Spring bypass begins existing 25 ms engage ramp instead of jumping to dry");
    auto off=p;off.enabled=false;dsp.setTarget(off);for(int n=0;n<100;++n){audio.clear();dsp.process(audio);}
    for(int c=0;c<2;++c)for(int i=0;i<96;++i)audio.setSample(c,i,(c?.03f:-.02f)*float(i));auto before=hash(audio);dsp.process(audio);check(hash(audio)==before,"Spring settled bypass retains distinct dry L/R bits");
    off.enabled=true;off.mixPercent=0;dsp.setTarget(off);for(int n=0;n<100;++n){audio.clear();dsp.process(audio);}audio.setSample(0,5,-0.f);audio.setSample(1,11,.47f);before=hash(audio);dsp.process(audio);check(hash(audio)==before,"Spring Mix zero preserves dry signed zero and samples exactly");
    dsp.prepare(96000,512);audio.clear();dsp.process(audio);check(finite(audio)&&energy(audio)==0,"Spring repeated preparation clears history and safely changes rate/block");
    // Exact old-engine oracle, frozen before integrating Spring. Active and OFF,
    // old types and A/B, variable callbacks, controls changing while playing.
    for(auto type:{Type::Room,Type::Plate,Type::Large})
    {
        tonegrid::ReverbProcessor current;tonegrid::reference::ReverbProcessor previous;
        auto target=p;target.type=type;target.mixPercent=17;target.preDelayMs=12;
        current.setTarget(target);previous.setTarget(target,tonegrid::reference::Candidate::Dattorro);current.prepare(48000,96);previous.prepare(48000,96);
        AudioBuffer<float> a(2,96),b(2,96);MemoryOutputStream aBytes,bBytes;bool equal=true;
        for(int block=0;block<400;++block)
        {
            if(block==80||block==180){target.timeSeconds=block==80?3.7f:1.5f;target.color=block==80?73.f:40.f;target.mixPercent=block==80?25.f:17.f;target.preDelayMs=block==80?37.5f:12.f;current.setTarget(target);previous.setTarget(target,tonegrid::reference::Candidate::Dattorro);}
            if(block==300){target.enabled=false;current.setTarget(target);previous.setTarget(target,tonegrid::reference::Candidate::Dattorro);}
            for(int i=0;i<96;++i){a.setSample(0,i,.07f*std::sin((block*96+i)*.037f));a.setSample(1,i,.04f*std::cos((block*96+i)*.051f));}b.makeCopyOf(a);current.process(a);previous.process(b);equal&=hash(a)==hash(b);
            for(int c=0;c<2;++c){aBytes.write(a.getReadPointer(c),96*sizeof(float));bBytes.write(b.getReadPointer(c),96*sizeof(float));}
        }
        const auto currentHash=SHA256(aBytes.getData(),aBytes.getDataSize()).toHexString();
        check(equal&&currentHash==SHA256(bBytes.getData(),bBytes.getDataSize()).toHexString(),"Exact frozen pre-Spring active/control/bypass fingerprint: candidate "+String(0)+" type "+String(int(type)));
        report+="OLD_REVERB_SHA256_"+String(0)+"_"+String(int(type))+"="+currentHash+"\n";
    }
    // Irregular host callbacks and repeated cross-engine transitions, no resize.
    std::vector<AudioBuffer<float>> buffers;for(int n:{1,3,31,32,95,96,257,512,1000})buffers.emplace_back(2,n);
    tonegrid::output::ReverbNode node;node.prepareToPlay(48000,96);dsp.prepare(48000,96);core.prepare(48000,96);const auto preparedC=core.preparationAllocations();bool safe=true;
    realtimeProbe::begin();
    for(int n=0;n<2000;++n)
    {
        auto t=p;t.type=Type(n%4);t.enabled=n%7!=0;t.timeSeconds=n%2?.2f:10.f;t.preDelayMs=n%2?0:100;t.color=n%2?0:100;t.mixPercent=n%2?0:100;
        auto& b=buffers[size_t(n)%buffers.size()];for(int i=0;i<b.getNumSamples();++i){b.setSample(0,i,.08f*std::sin(i*.037f));b.setSample(1,i,.02f*std::cos(i*.061f));}
        safe&=dsp.setTarget(t);dsp.process(b,n%13!=0);safe&=finite(b);
        safe&=node.configure(t);node.stereoOutputAvailable(n%17!=0);node.processBlock(b,midi);safe&=finite(b);
        core.setParameters(t.timeSeconds,t.color);const auto sample=core.processSample(.1f,-.03f);safe&=std::isfinite(sample[0])&&std::isfinite(sample[1]);
    }
    const auto allocations=realtimeProbe::end();check(allocations==0&&core.preparationAllocations()==preparedC,"Spring and combined node: zero C++/wrapped C allocations across 2000 rapid updates, type/bypass/mono/irregular-block transitions");
    check(safe,"Room-Spring-Plate-Large repeated handoff and extreme updates remain finite");
    // Interleaved separate instances: converters must never alias shared storage.
    tonegrid::SpringReverbProcessor other;other.prepare(44100,96);other.setParameters(2.1f,65);core.reset();other.reset();bool isolated=true;
    for(int i=0;i<10000;++i){core.processSample(.02f,.02f);auto sample=other.processSample(0,0);isolated&=sample[0]==0&&sample[1]==0;}
    check(isolated,"Independent Spring instances/rates retain isolated converter and tank storage");
    // New semantics are explicit; old type encodings remain byte-identical.
    tonegrid::output::Settings settings;settings.reverb=p;auto xml=tonegrid::output::encode(settings);tonegrid::output::Settings decoded;
    check(xml.getIntAttribute("version")==3&&xml.getChildByName("Reverb")->getIntAttribute("version")==2&&tonegrid::output::decode(xml,decoded)&&decoded.reverb.type==Type::Spring&&decoded.reverb.timeSeconds==p.timeSeconds,"Spring state uses OutputDSP 3/Reverb 2 and round-trips all controls");
    bool oldCodecSame=true;for(auto type:{Type::Room,Type::Plate,Type::Large}){settings.reverb.type=type;oldCodecSame&=tonegrid::output::encode(settings).toString()==tonegrid::reference::output::encode(settings).toString();}
    check(oldCodecSame,"Room/Plate/Large persistent OutputDSP XML stays byte-identical to frozen old codec");
    auto bad=xml;bad.setAttribute("version",2);bad.getChildByName("Reverb")->setAttribute("version",1);check(!tonegrid::output::decode(bad,decoded),"Old Reverb 1 cannot silently accept Spring ID 3");
    bad=xml;bad.getChildByName("Reverb")->setAttribute("type",4);check(!tonegrid::output::decode(bad,decoded),"Unknown future reverb enum rejected transactionally");
    auto state=board::migrations::initialState();state.name="Synthetic Spring";state.genericVst3=true;state.amp.append("opaque",6);settings.reverb=p;state.outputDsp=settings;
    PluginDescription desc;desc.name="Synthetic amp";desc.pluginFormatName="VST3";desc.fileOrIdentifier="synthetic.vst3";desc.manufacturerName="Synthetic";desc.uniqueId=123;desc.version="test";
    board::RigIdentityLookup identity=[](int i,int n){return "synthetic-"+String(i)+"-"+String(n);};auto temp=File::getSpecialLocation(File::tempDirectory).getNonexistentChildFile("ToneGridSpring","",true);auto store=board::RigStore::forApplicationData(temp);board::State loaded;PluginDescription required;
    auto result=store.save(0,state,desc,identity);auto rig=board::encode(state,desc,identity);
    check(result.result.wasOk()&&rig.getIntAttribute("version")==11&&store.loadForSelection(0,identity,loaded,required).wasOk()&&loaded.outputDsp&&loaded.outputDsp->reverb.type==Type::Spring&&loaded.amp==state.amp,"Spring temporary rig 11 storage restores controls and exact opaque VST bytes");
    auto wrong=rig;wrong.setAttribute("version",10);check(board::decode(wrong,desc,identity,loaded).failed(),"Rig 10 cannot smuggle new Spring OutputDSP 3 semantics");
    state.outputDsp->reverb.type=Type::Room;auto oldRig=board::encode(state,desc,identity);check(oldRig.getIntAttribute("version")==10&&board::decode(oldRig,desc,identity,loaded).wasOk()&&loaded.outputDsp->reverb.type==Type::Room,"Existing rig 10 stays rig 10 and never auto-selects Spring");
    rig::writeXml(oldRig,store.slotFile(1));const auto oldFile=store.slotFile(1).loadFileAsString();store.loadForSelection(1,identity,loaded,required);check(oldFile==store.slotFile(1).loadFileAsString(),"Old rig load does not rewrite temporary storage");
    check(temp.deleteRecursively(),"All Spring synthetic storage removed");
    tonegrid::ReverbControls ui;ui.sync(p);auto* type=dynamic_cast<ComboBox*>(ui.findChildWithID("reverb-type"));auto* candidate=dynamic_cast<ComboBox*>(ui.findChildWithID("reverb-candidate"));
    check(type&&type->getNumItems()==4&&type->getSelectedId()==4&&candidate==nullptr&&ui.state(true).type==Type::Spring,"Existing popup adds Spring only, has no development selector");
    check(tonegrid::reverbHelp("reverb-type",Type::Spring).contains("drip")&&tonegrid::reverbHelp("reverb-time",Type::Spring).contains("spring tail"),"Spring musician help describes drip/splash and musical decay");
    if(type)type->setSelectedId(2,sendNotificationSync);check(candidate==nullptr&&ui.state(true).type==Type::Plate&&ui.state(true).mixPercent==100,"Spring-to-Plate popup transition preserves controls without resetting common knobs");
    report+=String(checks)+" Spring integration/regression/RT/state/UI checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
