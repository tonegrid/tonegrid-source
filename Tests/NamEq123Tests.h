#pragma once
#include "Effects/AmpEq/AmpEqProcessor.h"
#include "OutputDspState.h"
#include "Content/Presets/RigCodec.h"
#include "Effects/AmpEq/AdvancedEqControls.h"
#include "RealtimeAllocationProbe.h"
#include "Reference/NamEq123LegacyProcessor.h"
#include <cstring>
namespace namEq123Tests
{
inline bool run(juce::String& report)
{
    using namespace juce;using namespace tonegrid;bool ok=true;int checks=0;
    auto check=[&](bool pass,const String& name){++checks;ok&=pass;report+=String(pass?"PASS ":"FAIL ")+"NAM EQ123 "+name+"\n";};
    // Frozen pre-Depth processor is an independent, same-toolchain sonic oracle.
    for(double rate:{44100.,48000.,96000.})
    {
        ampEq::Processor current;legacyEq123::Processor previous;current.prepare(rate);previous.prepare(rate);
        AudioBuffer<float> a(2,257),b(2,257);bool exact=true;
        for(int block=0;block<200;++block)
        {
            ampEq::Parameters p;p.bassDb=block<30?0:3;p.midDb=-2;p.trebleDb=4;p.lowCut=block>60;p.highCut=block>80;p.lowMidDb=2;p.presenceDb=block<100?-3:6;p.presenceHz=2600;p.presenceQ=.7f;
            current.setTarget(p,true);previous.setTarget(p,true);
            for(int c=0;c<2;++c)for(int i=0;i<a.getNumSamples();++i)a.setSample(c,i,float(.01*std::sin((block*257+i)*.071))*(c?-1:1));
            b.makeCopyOf(a);current.process(a);previous.process(b);
            for(int c=0;c<2;++c)exact&=std::memcmp(a.getReadPointer(c),b.getReadPointer(c),257*sizeof(float))==0;
        }
        check(exact,"Depth=0 is bit-exact to pre-change EQ with existing bands/cuts at "+String(rate));
    }
    const auto response=[](float db,double hz,double rate)
    {
        ampEq::Parameters p;p.depthDb=db;ampEq::Processor dsp;dsp.prepare(rate);dsp.setTarget(p,true);
        AudioBuffer<float> b(2,257);double input=0,output=0;
        for(int n=0;n<450;++n){for(int i=0;i<257;++i){float x=float(.01*std::sin(2*MathConstants<double>::pi*hz*(n*257+i)/rate));b.setSample(0,i,x);b.setSample(1,i,-x);if(n>200)input+=double(x)*x;}dsp.process(b);if(n>200)for(int i=0;i<257;++i)output+=double(b.getSample(0,i))*b.getSample(0,i);}
        return 10*std::log10(output/input);
    };
    for(double rate:{44100.,48000.,96000.})for(float db:{-6.f,6.f})
    {
        check(std::abs(response(db,10,rate)-db)<.04,"Depth shelf low-frequency gain "+String(db)+" / "+String(rate));
        check(std::abs(response(db,100,rate)-db*.5)<.02,"Depth shelf half gain at 100 Hz / "+String(rate));
        check(std::abs(response(db,10000,rate))<.01,"Depth preserves high frequencies / "+String(rate));
    }
    output::Settings settings;settings.ampEq.presenceDb=4;settings.ampEq.presenceHz=2600;settings.ampEq.presenceQ=1.7f;
    const auto old=output::encode(settings);output::Settings decoded;
    check(old.getIntAttribute("version")==6&&output::decode(old,decoded)&&decoded.ampEq.depthDb==0&&decoded.ampEq.depthHz==100&&decoded.ampEq.presenceDb==4&&decoded.ampEq.presenceQ==1.7f,"legacy advanced state gets neutral Depth and retains Presence");
    settings.ampEq.depthDb=3.5f;settings.ampEq.depthHz=140;const auto modern=output::encode(settings);
    check(modern.getIntAttribute("version")==7&&modern.getChildByName("AmpEq")->getIntAttribute("version")==3&&output::decode(modern,decoded)&&decoded.ampEq.depthDb==3.5f&&decoded.ampEq.depthHz==140,"Depth output state v7 / AmpEq v3 round trip");
    for(const auto bad:{"nan","inf","6.1","-6.1","","3bad"}){auto x=modern;x.getChildByName("AmpEq")->setAttribute("depthDb",bad);auto sentinel=decoded;check(!output::decode(x,sentinel)&&sentinel.ampEq.depthDb==3.5f,"invalid Depth gain rejected transactionally: "+String(bad));}
    for(const auto bad:{"nan","39","251",""}){auto x=modern;x.getChildByName("AmpEq")->setAttribute("depthHz",bad);check(!output::decode(x,decoded),"invalid Depth frequency rejected: "+String(bad));}
    auto missing=modern;missing.getChildByName("AmpEq")->removeAttribute("depthHz");check(!output::decode(missing,decoded),"new state cannot silently drop Depth frequency");
    board::State state;state.sourceType=amp::SourceType::Nam;state.name="Depth preset";state.namAmp.model={"Test.nam",String::repeatedString("a",64),128};state.outputDsp=settings;
    for(int p=0;p<board::count;++p)state.pedals[size_t(p)]=board::defaults(p,0);
    board::RigIdentityLookup identity=[](int,int){return String::repeatedString("b",64);};board::State restored;
    auto rig=board::encode(state,{},identity);check(rig.getIntAttribute("version")==16&&board::decode(rig,{},identity,restored).wasOk()&&restored.outputDsp->ampEq.depthDb==3.5f,"rig v16 persists Depth");
    state.pedals[3].preset=board::presetCount(3);state.customPedalHashes[3]=String::repeatedString("c",64);rig=board::encode(state,{},identity);
    check(board::decode(rig,{},identity,restored).wasOk()&&restored.customPedalHashes[3]==state.customPedalHashes[3],"Depth rig retains personal pedal hash");
    ampEq::Processor dsp;dsp.prepare(48000);AudioBuffer<float> audio(2,257);bool finite=true;float last=.01f;double step=0;
    realtimeProbe::begin();
    for(int n=0;n<1000;++n)
    {
        ampEq::Parameters p;p.depthDb=(n/20)%2?6:-6;p.depthHz=(n/30)%2?40:250;p.lowMidDb=2;p.presenceDb=-2;
        dsp.setTarget(p,n%79!=0);for(int c=0;c<2;++c)for(int i=0;i<257;++i)audio.setSample(c,i,.01f);
        dsp.process(audio);for(int i=0;i<257;++i){const float x=audio.getSample(0,i);finite&=std::isfinite(x);last=x;}
    }
    const auto allocations=realtimeProbe::end();check(finite&&allocations==0,"rapid Depth/filter/NAM/VST transitions finite, zero callback allocations");
    dsp.prepare(48000);last=.01f;
    for(int n=0;n<70;++n){ampEq::Parameters p;p.depthDb=n<2?0:6;dsp.setTarget(p,true);for(int c=0;c<2;++c)for(int i=0;i<257;++i)audio.setSample(c,i,.01f);dsp.process(audio);for(int i=0;i<257;++i){const float x=audio.getSample(0,i);step=std::max(step,std::abs(double(x-last)));last=x;}}
    check(step<.0001&&std::abs(last-.0199526)<1.e-5,"Depth gain smoothly ramps on DC without a level jump");
    dsp.setTarget(settings.ampEq,false);audio.clear();audio.setSample(0,1,-.0123f);audio.setSample(1,1,.0345f);dsp.process(audio);check(audio.getSample(0,1)==-.0123f&&audio.getSample(1,1)==.0345f,"VST3 bypass remains exact with nonzero Depth");
    ampui::AdvancedEqControls controls;controls.sync(settings.ampEq);
    check(!controls.advancedOpen()&&controls.getHeight()==controls.collapsedHeight&&controls.state().presenceDb==4&&controls.state().depthDb==3.5f,"new EQ maps existing Presence and Depth");
    const auto before=output::encode(settings).toString();int publishes=0;controls.onChange=[&](auto){++publishes;};controls.setAdvancedOpen(true);controls.setAdvancedOpen(false);
    check(publishes==0&&output::encode(settings).toString()==before,"band selection never publishes audio changes");
    auto* reset=dynamic_cast<Button*>(controls.findChildWithID("eq-reset"));if(reset)reset->onClick();else check(false,"Reset available");const auto p=controls.state();
    check(ampEq::flat(p)&&!ampEq::advanced(p)&&p.depthHz==100&&p.enabled&&publishes==1,"Reset restores every EQ default");
    if(!ampEq::flat(p)||ampEq::advanced(p)||p.depthHz!=100||!p.enabled||publishes!=1)report+="RESET_DIAGNOSTIC bass="+String(p.bassDb,18)+" mid="+String(p.midDb,18)+" treble="+String(p.trebleDb,18)+" presence="+String(p.presenceDb,18)+" depth="+String(p.depthDb,18)+" depthHz="+String(p.depthHz,18)+" enabled="+String(int(p.enabled))+" lowCut="+String(int(p.lowCut))+" highCut="+String(int(p.highCut))+" publishes="+String(publishes)+"\n";
    report+=String(checks)+" NAM EQ123 checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
