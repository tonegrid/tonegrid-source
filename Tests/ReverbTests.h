#pragma once
#include "Effects/Reverb/ReverbProcessor.h"
#include "Effects/Reverb/ReverbControls.h"
#include "OutputDspNodes.h"
#include "Content/Presets/RigStore.h"
#include "SelfTests.h"
#include "RealtimeAllocationProbe.h"
namespace reverbTests
{
using namespace juce;using tonegrid::reverb::Parameters;using tonegrid::reverb::Type;
inline AudioBuffer<float> impulse(Parameters p,int channels=2,double rate=48000)
{
    tonegrid::ReverbProcessor dsp;dsp.setTarget(p);dsp.prepare(rate,96);
    AudioBuffer<float> warm(channels,int(rate*.1));warm.clear();dsp.process(warm);
    AudioBuffer<float> audio(channels,int(rate*3));audio.clear();audio.setSample(0,0,1.f);if(channels>1)audio.setSample(1,0,1.f);
    dsp.process(audio);return audio;
}
inline double energy(const AudioBuffer<float>& audio,int first=0)
{double e=0;for(int c=0;c<audio.getNumChannels();++c)for(int i=first;i<audio.getNumSamples();++i)e+=double(audio.getSample(c,i))*audio.getSample(c,i);return e;}
inline int onset(const AudioBuffer<float>& audio)
{for(int i=0;i<audio.getNumSamples();++i)if(std::abs(audio.getSample(0,i))>1e-7f)return i;return -1;}
inline bool finite(const AudioBuffer<float>& b)
{for(int c=0;c<b.getNumChannels();++c)for(int i=0;i<b.getNumSamples();++i)if(!std::isfinite(b.getSample(c,i)))return false;return true;}
inline String hash(const AudioBuffer<float>& b)
{MemoryOutputStream m;for(int c=0;c<b.getNumChannels();++c)m.write(b.getReadPointer(c),size_t(b.getNumSamples())*sizeof(float));return SHA256(m.getData(),m.getDataSize()).toHexString();}
inline double highFrequencyRatio(const AudioBuffer<float>& b)
{double difference=0;for(int c=0;c<b.getNumChannels();++c)for(int i=1;i<b.getNumSamples();++i){const double d=b.getSample(c,i)-b.getSample(c,i-1);difference+=d*d;}return difference/std::max(1.e-20,energy(b));}
inline bool run(String& report)
{
    bool ok=true;int checks=0;auto check=[&](bool value,const String& label){++checks;ok&=value;report+=String(value?"PASS ":"FAIL ")+label+"\n";};
    Parameters p;check(!p.enabled&&p.type==Type::Room&&p.timeSeconds==1.5f&&p.preDelayMs==12&&p.color==40&&p.mixPercent==15,"Reverb conservative defaults and legacy OFF");
    tonegrid::ReverbProcessor unprepared;AudioBuffer<float> source(2,97);for(int c=0;c<2;++c)for(int i=0;i<97;++i)source.setSample(c,i,float(c+1)*.01f*float(i-41));auto expected=hash(source);unprepared.process(source);check(hash(source)==expected,"Reverb before prepare is transparent and safe");
    bool reject=true;for(int field=0;field<6;++field){auto bad=p;switch(field){case 0:bad.timeSeconds=NAN;break;case 1:bad.timeSeconds=11;break;case 2:bad.preDelayMs=-1;break;case 3:bad.color=101;break;case 4:bad.mixPercent=INFINITY;break;default:bad.type=Type(4);break;}reject&=!unprepared.setTarget(bad);}
    check(reject,"Nonfinite/range/enum reverb parameter targets rejected transactionally");
    bool badPrepare=false;try{unprepared.prepare(0,96);}catch(const std::invalid_argument&){badPrepare=true;}check(badPrepare,"Invalid preparation is rejected outside callback");
    {
        const String label="Dattorro ";p.enabled=true;p.mixPercent=100;p.preDelayMs=0;
        auto base=impulse(p);check(finite(base)&&energy(base)>1.e-7,"Reverb "+label+"finite wet impulse and nonzero tail");
        double stereo=0;for(int i=0;i<base.getNumSamples();++i)stereo+=std::abs(base.getSample(0,i)-base.getSample(1,i));check(stereo>1.e-5,"Reverb "+label+"mono excitation generates stereo tail");
        auto mono=impulse(p,1);bool collapse=true;for(int i=0;i<mono.getNumSamples();++i)collapse&=std::abs(mono.getSample(0,i)-(base.getSample(0,i)+base.getSample(1,i))*.5f)<1.e-6;check(collapse,"Reverb "+label+"one-channel contract averages wet L/R safely");
        tonegrid::ReverbProcessor deviceMono;deviceMono.setTarget(p);deviceMono.prepare(48000,96);AudioBuffer<float> monoWarm(2,4800);monoWarm.clear();deviceMono.process(monoWarm,false);AudioBuffer<float> physical(2,base.getNumSamples());physical.clear();physical.setSample(0,0,1);physical.setSample(1,0,1);deviceMono.process(physical,false);bool deviceCollapse=true;for(int i=0;i<physical.getNumSamples();++i)deviceCollapse&=physical.getSample(0,i)==physical.getSample(1,i)&&std::abs(physical.getSample(0,i)-mono.getSample(0,i))<1.e-6;check(deviceCollapse,"Reverb "+label+"physical mono device receives average wet even in a two-channel host graph");
        auto delayed=p;delayed.preDelayMs=100;auto later=impulse(delayed);check(onset(later)-onset(base)==4800,"Reverb "+label+"100 ms pre-delay shifts wet onset by 4800 samples");
        auto shortP=p;shortP.timeSeconds=.2f;auto longP=p;longP.timeSeconds=10;const auto shortTail=impulse(shortP),longTail=impulse(longP);check(finite(shortTail)&&finite(longTail)&&energy(longTail,48000)>energy(shortTail,48000)*2,"Reverb "+label+"Time extremes are finite and long tails retain more energy");
        auto dark=p;dark.color=0;auto bright=p;bright.color=100;auto darkIR=impulse(dark),brightIR=impulse(bright);check(finite(darkIR)&&finite(brightIR)&&hash(darkIR)!=hash(brightIR),"Reverb "+label+"Color changes wet spectral response");
        check(highFrequencyRatio(brightIR)>highFrequencyRatio(darkIR),"Reverb "+label+"bright Color increases normalized high-frequency energy");
        auto plate=p;plate.type=Type::Plate;auto large=p;large.type=Type::Large;check(hash(base)!=hash(impulse(plate))&&hash(base)!=hash(impulse(large))&&hash(impulse(plate))!=hash(impulse(large)),"Reverb "+label+"Room/Plate/Large produce distinct voicings");
        for(auto rate:{44100.,96000.,192000.}){auto ir=impulse(p,2,rate);check(finite(ir)&&energy(ir)>1e-8,"Reverb "+label+"sample-rate scaled impulse at "+String(rate));}
        tonegrid::ReverbProcessor dsp;dsp.setTarget(p);dsp.prepare(48000,96);AudioBuffer<float> b(2,256);b.clear();for(int n=0;n<30;++n)dsp.process(b);
        auto off=p;off.enabled=false;dsp.setTarget(off);b.clear();b.setSample(0,0,.5f);dsp.process(b);
        const float transitional=b.getSample(0,0);check(transitional<.5f&&transitional>0,"Reverb "+label+"disable begins a ramp instead of immediate dry switch");
        for(int n=0;n<30;++n){b.clear();dsp.process(b);}for(int i=0;i<256;++i){b.setSample(0,i,.07f*std::sin(i*.07f));b.setSample(1,i,-.03f*std::cos(i*.03f));}expected=hash(b);dsp.process(b);check(hash(b)==expected,"Reverb "+label+"settled bypass preserves distinct L/R bits");
        off.enabled=true;off.mixPercent=0;dsp.setTarget(off);for(int n=0;n<30;++n){b.clear();dsp.process(b);}for(int i=0;i<256;++i){b.setSample(0,i,.11f*std::sin(i*.031f));b.setSample(1,i,-.02f);}expected=hash(b);dsp.process(b);check(hash(b)==expected,"Reverb "+label+"Mix zero is bit-exact after smoothing");
        dsp.setTarget(p);dsp.reset();b.clear();for(int n=0;n<120;++n)dsp.process(b);check(energy(b)==0,"Reverb "+label+"reset clears wet history to exact silence");
        dsp.prepare(96000,512);b.clear();dsp.process(b);check(finite(b)&&energy(b)==0,"Reverb "+label+"re-prepare is safe and clears history");
        tonegrid::ReverbProcessor chunks;chunks.setTarget(p);chunks.prepare(48000,96);AudioBuffer<float> warm(2,4800);warm.clear();chunks.process(warm);AudioBuffer<float> chunked(2,base.getNumSamples());chunked.clear();int position=0;constexpr std::array<int,5> sizes{1,96,37,512,255};int part=0;
        while(position<chunked.getNumSamples()){const auto n=std::min(sizes[size_t(part++)%sizes.size()],chunked.getNumSamples()-position);AudioBuffer<float> block(2,n);block.clear();if(position==0){block.setSample(0,0,1);block.setSample(1,0,1);}chunks.process(block);for(int c=0;c<2;++c)chunked.copyFrom(c,position,block,c,0,n);position+=n;}
        check(hash(chunked)==hash(base),"Reverb "+label+"sample processing is bit-identical across variable callback partitions");
        dsp.prepare(48000,96);dsp.setTarget(p);dsp.reset();warm.clear();dsp.process(warm);for(int i=0;i<256;++i){b.setSample(0,i,.07f*std::sin(i*.07f));b.setSample(1,i,-b.getSample(0,i));}dsp.process(b);check(energy(b)==0,"Reverb "+label+"wet tank mono sum explicitly cancels anti-phase stereo input");
        tonegrid::ReverbProcessor beforeChange,afterChange;beforeChange.setTarget(p);afterChange.setTarget(p);beforeChange.prepare(48000,96);afterChange.prepare(48000,96);AudioBuffer<float> x(2,96),y(2,96);
        for(int block=0;block<100;++block){for(int i=0;i<96;++i){x.setSample(0,i,.05f*std::sin((block*96+i)*.071f));x.setSample(1,i,.03f*std::cos((block*96+i)*.043f));}y.makeCopyOf(x);beforeChange.process(x);afterChange.process(y);}
        auto changed=p;changed.type=Type::Large;changed.timeSeconds=10;changed.color=0;changed.preDelayMs=100;changed.mixPercent=0;afterChange.setTarget(changed);
        for(int i=0;i<96;++i){x.setSample(0,i,.05f*std::sin((9600+i)*.071f));x.setSample(1,i,.03f*std::cos((9600+i)*.043f));}y.makeCopyOf(x);beforeChange.process(x);afterChange.process(y);
        check(finite(y)&&std::abs(x.getSample(0,0)-y.getSample(0,0))<.001f,"Reverb "+label+"simultaneous Type/Time/Color/Pre-delay/Mix step starts continuously");
    }
    // Allocation probe covers entire DSP including setters and transitions, and the graph adapter.
    tonegrid::ReverbProcessor stress;tonegrid::output::ReverbNode node;stress.prepare(48000,96);node.prepareToPlay(48000,96);MidiBuffer midi;
    std::vector<AudioBuffer<float>> buffers;for(int size:{1,3,95,96,128,256,511,1000})buffers.emplace_back(2,size);
    bool safe=true;realtimeProbe::begin();
    for(int n=0;n<2000;++n)
    {
        p={n%5!=0,Type(n%3),n%2?.2f:10.f,n%2?0.f:100.f,n%2?0.f:100.f,n%2?0.f:100.f};
        auto& b=buffers[size_t(n)%buffers.size()];for(int i=0;i<b.getNumSamples();++i){b.setSample(0,i,.1f*std::sin(i*.02f));b.setSample(1,i,-.07f*std::cos(i*.05f));}
        safe&=stress.setTarget(p);stress.process(b);safe&=finite(b);
        safe&=node.configure(p);node.stereoOutputAvailable(n%19!=0);node.processBlock(b,midi);safe&=finite(b);
    }
    const auto allocations=realtimeProbe::end();check(allocations==0,"Reverb DSP + node: zero C++ allocations across 2000 rapid Time/Color/Mix/Type/engine/ON-OFF updates and oversized callbacks");check(safe,"Reverb extreme alternating parameters and Dattorro engine remain finite across 2000 callbacks");
    // A single actual stochastic Spread render avoids comparing two random seeds.
    tonegrid::StereoSpreadProcessor stochastic;stochastic.prepare(48000,96);stochastic.setTarget({15,.25f,130,true,true},true);tonegrid::ReverbProcessor bypass;bypass.prepare(48000,96);AudioBuffer<float> w(2,96);bool sameRandom=true;
    for(int block=0;block<100;++block){for(int i=0;i<96;++i){w.setSample(0,i,.07f*std::sin((block*96+i)*.03f));w.setSample(1,i,0);}stochastic.process(w);const auto before=hash(w);bypass.process(w);sameRandom&=before==hash(w);}check(sameRandom,"Reverb OFF preserves the exact samples of actual stochastic Wobble-enabled Spread");
    const auto monoBypass=hash(w);bypass.process(w,false);check(hash(w)==monoBypass,"Physical mono mode keeps bypass dry L/R bit-exact, without summing upstream stereo dry signal");
    // Deterministic complete post-amp fingerprint, new node OFF vs absent baseline node.
    for(int variant=0;variant<4;++variant)
    {
        rig::StereoGraph a,b;auto setup=[&](rig::StereoGraph& graph,bool reverb)
        {
            graph.setPlayConfigDetails(2,2,48000,256);graph.setNonRealtime(true);using IO=AudioProcessorGraph::AudioGraphIOProcessor;
            auto in=graph.addNode(std::make_unique<IO>(IO::audioInputNode));auto amp=graph.addNode(std::make_unique<TestEffect>(1.5f,.02f));auto norm=std::make_unique<tonegrid::output::NormalizationNode>();norm->configure(variant&1,true,-24);auto n=graph.addNode(std::move(norm));auto spread=std::make_unique<tonegrid::output::SpreadNode>();tonegrid::output::Settings s;s.spreadEnabled=(variant&2)!=0;s.wobbleEnabled=false;s.offsetMs=-9.5f;spread->configure(s);auto wide=graph.addNode(std::move(spread));auto out=graph.addNode(std::make_unique<IO>(IO::audioOutputNode));
            auto final=wide;if(reverb)final=graph.addNode(std::make_unique<tonegrid::output::ReverbNode>());
            bool connected=true;for(int c=0;c<2;++c){connected&=graph.addConnection({{in->nodeID,c},{amp->nodeID,c}})&&graph.addConnection({{amp->nodeID,c},{n->nodeID,c}})&&graph.addConnection({{n->nodeID,c},{wide->nodeID,c}});if(reverb)connected&=graph.addConnection({{wide->nodeID,c},{final->nodeID,c}});connected&=graph.addConnection({{final->nodeID,c},{out->nodeID,c}});}graph.prepareToPlay(48000,256);return connected;
        };
        bool identical=setup(a,false)&&setup(b,true);MemoryOutputStream oldRender,newRender;AudioBuffer<float> aa(2,256),bb(2,256);
        for(int block=0;block<100;++block){for(int i=0;i<256;++i){float x=.08f*std::sin((block*256+i)*.037f);aa.setSample(0,i,x);aa.setSample(1,i,-x*.7f);}bb.makeCopyOf(aa);a.processBlock(aa,midi);b.processBlock(bb,midi);for(int c=0;c<2;++c){oldRender.write(aa.getReadPointer(c),256*sizeof(float));newRender.write(bb.getReadPointer(c),256*sizeof(float));}}
        const auto oldHash=SHA256(oldRender.getData(),oldRender.getDataSize()).toHexString(),newHash=SHA256(newRender.getData(),newRender.getDataSize()).toHexString();check(identical&&oldHash==newHash,"Reverb OFF post-amp graph fingerprint matches validated topology, Normalize/Spread variant "+String(variant));report+="POST_FX_OFF_SHA256_"+String(variant)+"="+newHash+"\n";
    }
    // Pure codec + temporary storage, no real user folder.
    tonegrid::output::Settings controls;controls.spreadEnabled=true;controls.reverb={true,Type::Large,7.2f,37.5f,28,23};auto xml=tonegrid::output::encode(controls);tonegrid::output::Settings decoded;
    check(tonegrid::output::decode(xml,decoded)&&decoded.reverb.enabled&&decoded.reverb.type==Type::Large&&decoded.reverb.timeSeconds==7.2f&&decoded.reverb.preDelayMs==37.5f&&decoded.reverb.color==28&&decoded.reverb.mixPercent==23,"Reverb strict codec round-trips all six persistent controls");
    bool transactional=true;for(auto field:{"enabled","type","timeSeconds","preDelayMs","color","mixPercent"}){auto broken=xml;broken.getChildByName("Reverb")->removeAttribute(field);decoded=controls;transactional&=!tonegrid::output::decode(broken,decoded)&&decoded.reverb.enabled&&decoded.reverb.timeSeconds==7.2f;}check(transactional,"Reverb missing fields reject without partially publishing state");
    bool ranges=true;for(auto field:{"timeSeconds","preDelayMs","color","mixPercent"})for(auto value:{"nan","inf","12junk","-1","101"}){auto broken=xml;broken.getChildByName("Reverb")->setAttribute(field,value);ranges&=!tonegrid::output::decode(broken,decoded);}check(ranges,"Reverb corrupt/nonfinite/out-of-range persistent controls rejected");
    auto old=xml;old.setAttribute("version",1);old.removeChildElement(old.getChildByName("Reverb"),true);check(tonegrid::output::decode(old,decoded)&&!decoded.reverb.enabled&&decoded.spreadEnabled,"Historical OutputDSP v1 reads with Reverb OFF and Spread unchanged");
    auto missing=xml;missing.removeChildElement(missing.getChildByName("Reverb"),true);check(!tonegrid::output::decode(missing,decoded),"OutputDSP v2 requires Reverb section");auto duplicate=xml;duplicate.addChildElement(new XmlElement(*xml.getChildByName("Reverb")));check(!tonegrid::output::decode(duplicate,decoded),"Duplicate Reverb state rejected");
    const auto temp=File::getSpecialLocation(File::tempDirectory),root=temp.getChildFile("ToneGrid-reverb-tests-"+Uuid().toString());ScopeGuard cleanup{[&]{if(root.isAChildOf(temp))root.deleteRecursively();}};auto store=board::RigStore::forApplicationData(root);
    auto state=board::migrations::initialState();state.name="Synthetic reverb";state.genericVst3=true;state.amp.append("opaque",6);state.outputDsp=controls;
    PluginDescription description;description.name="Synthetic amp";description.pluginFormatName="VST3";description.fileOrIdentifier="synthetic.vst3";description.manufacturerName="Synthetic";description.uniqueId=123;description.version="test";board::RigIdentityLookup identity=[](int p,int n){return "synthetic-"+String(p)+"-"+String(n);};board::State loaded;PluginDescription required;
    check(store.save(0,state,description,identity).result.wasOk()&&store.loadForSelection(0,identity,loaded,required).wasOk()&&loaded.outputDsp&&loaded.outputDsp->reverb.enabled&&loaded.amp==state.amp,"Reverb v10 temporary storage round-trip preserves exact opaque VST state");
    auto rig=board::encode(state,description,identity);check(rig.getIntAttribute("version")==10&&!rig.toString().contains("candidate")&&!rig.toString().contains("Dattorro"),"Public rig format 10 excludes development candidate choice");
    auto v9=rig;v9.setAttribute("version",9);v9.removeChildElement(v9.getChildByName("OutputDSP"),true);v9.addChildElement(new XmlElement(old));check(rig::writeXml(v9,store.slotFile(1)),"Seed historical v9 only in synthetic directory");const auto before=store.slotFile(1).loadFileAsString();check(store.loadForSelection(1,identity,loaded,required).wasOk()&&loaded.outputDsp&&!loaded.outputDsp->reverb.enabled&&loaded.outputDsp->spreadEnabled&&store.slotFile(1).loadFileAsString()==before,"Historical v9 read stays byte-identical on disk; Reverb OFF without rewriting");
    auto mismatched=rig;mismatched.setAttribute("version",9);check(board::decode(mismatched,description,identity,loaded).failed(),"Rig v9 cannot smuggle new v2 output semantics");
    auto invalid=state;invalid.outputDsp->reverb.timeSeconds=NAN;const auto saved=store.slotFile(0).loadFileAsString();check(store.save(0,invalid,description,identity).result.failed()&&store.slotFile(0).loadFileAsString()==saved,"Invalid reverb Save leaves previous synthetic rig intact");
    tonegrid::ReverbControls ui;ui.sync(controls.reverb);check(ui.state(true).timeSeconds==7.2f&&ui.state(true).type==Type::Large&&ui.findChildWithID("reverb-candidate")==nullptr,"Reverb popup syncs common parameters and no development selector");
    auto* type=dynamic_cast<ComboBox*>(ui.findChildWithID("reverb-type"));auto* mix=dynamic_cast<Slider*>(ui.findChildWithID("reverb-mix"));bool changed=false;ui.onChange=[&]{changed=true;};if(type)type->setSelectedId(2,sendNotificationSync);check(type&&mix&&changed&&ui.state(true).type==Type::Plate&&ui.getLocalBounds().contains(mix->getBounds()),"Actual popup controls publish changes and fit left-column layout");
    report+=String(checks)+" reverb DSP/graph/state/RT/UI checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
