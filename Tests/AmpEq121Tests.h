#pragma once
#include "Effects/AmpEq/AmpEqProcessor.h"
#include "OutputDspNodes.h"
#include "Content/Presets/RigCodec.h"
#include "RealtimeAllocationProbe.h"
#include "CompactAmpLayout.h"
#include <cstring>
namespace ampEq121Tests
{
inline bool run(juce::String& report)
{
    using namespace juce;using namespace tonegrid;bool ok=true;int checks=0;
    auto check=[&](bool pass,const String& message){++checks;ok&=pass;report+=String(pass?"PASS ":"FAIL ")+"EQ121 "+message+"\n";};
    const auto response=[](ampEq::Parameters p,double hz,double rate)
    {
        ampEq::Processor eq;eq.prepare(rate);eq.setTarget(p,true);AudioBuffer<float> b(2,128);double energy=0,input=0;
        for(int block=0;block<400;++block){for(int i=0;i<128;++i){float x=float(.01*std::sin(2*MathConstants<double>::pi*hz*(block*128+i)/rate));b.setSample(0,i,x);b.setSample(1,i,-x);if(block>=200)input+=double(x)*x;}eq.process(b);if(block>=200)for(int i=0;i<128;++i)energy+=double(b.getSample(0,i))*b.getSample(0,i);}
        return 10*std::log10(energy/input);
    };
    for(double rate:{44100.,48000.,96000.})
    {
        check(std::abs(response({6,0,0},10,rate)-6)<.03,"bass shelf +6 dB at 10 Hz / "+String(rate));
        check(std::abs(response({0,6,0},425,rate)-6)<.01,"mid bell +6 dB at 425 Hz / "+String(rate));
        check(std::abs(response({0,-6,0},425,rate)+6)<.01,"mid bell -6 dB at 425 Hz / "+String(rate));
        check(std::abs(response({0,0,6},18000,rate)-6)<.03,"treble shelf +6 dB at 18 kHz / "+String(rate));
        check(std::abs(response({6,0,0},12000,rate))<.03&&std::abs(response({0,0,6},20,rate))<.03,"shelves preserve opposite frequency range / "+String(rate));
    }
    ampEq::Processor dsp;dsp.prepare(48000);dsp.setTarget({},true);AudioBuffer<float> audio(2,257),original(2,257);
    for(int c=0;c<2;++c)for(int i=0;i<257;++i)audio.setSample(c,i,i==0?-0.f:float((c?-.37:.23)*std::sin(i*.23)));
    original.makeCopyOf(audio);dsp.process(audio);
    auto exact=[&]{return std::memcmp(audio.getReadPointer(0),original.getReadPointer(0),257*sizeof(float))==0&&std::memcmp(audio.getReadPointer(1),original.getReadPointer(1),257*sizeof(float))==0;};
    check(exact(),"neutral NAM EQ is bit-exact stereo unity, including signed zero");
    dsp.setTarget({12,-12,12},true);dsp.process(audio);dsp.setTarget({12,-12,12},false);audio.makeCopyOf(original);dsp.process(audio);check(exact(),"NAM to VST bypass immediately removes EQ and history with nonzero stored settings");
    output::AmpEqNode node;node.prepareToPlay(48000,257);node.configure({12,12,12},false);audio.makeCopyOf(original);MidiBuffer midi;node.processBlock(audio,midi);check(exact(),"graph adapter bypass is also bit-exact for VST3");
    check(!node.configure({NAN,0,0},true)&&!node.configure({0,INFINITY,0},true)&&!node.configure({0,0,12.1f},true),"invalid or out-of-range settings rejected before publication");
    bool finite=true;double biggestStep=0;float last=0;dsp.prepare(48000);
    realtimeProbe::begin();
    for(int block=0;block<1000;++block)
    {
        dsp.setTarget(block%7==0?ampEq::Parameters{12,-12,12}:ampEq::Parameters{-12,12,-12},block%19!=0);
        for(int c=0;c<2;++c)for(int i=0;i<257;++i)audio.setSample(c,i,.01f*float(std::sin((block*257+i)*.06)));
        dsp.process(audio);for(int i=0;i<257;++i){const float x=audio.getSample(0,i);finite&=std::isfinite(x);biggestStep=std::max(biggestStep,std::abs(double(x-last)));last=x;}
    }
    const auto allocations=realtimeProbe::end();check(allocations==0&&finite,"rapid EQ and NAM/VST transitions allocate nothing and remain finite");
    // Constant input isolates gain changes from normal waveform derivatives.
    dsp.prepare(48000);dsp.setTarget({},true);last=.01f;biggestStep=0;
    for(int block=0;block<70;++block){if(block==2)dsp.setTarget({12,0,0},true);for(int c=0;c<2;++c)for(int i=0;i<257;++i)audio.setSample(c,i,.01f);dsp.process(audio);for(int i=0;i<257;++i){const float x=audio.getSample(0,i);biggestStep=std::max(biggestStep,std::abs(double(x-last)));last=x;}}
    check(biggestStep<.0003&&std::abs(last-.0398107)<1.e-5,"+12 dB change ramps smoothly rather than jumping on constant input");
    dsp.setTarget({},true);for(int n=0;n<20;++n)dsp.process(audio);audio.makeCopyOf(original);dsp.process(audio);check(exact(),"reset to neutral returns to bit-exact unity after ramp");
    output::Settings old;output::Settings decoded;auto legacy=output::encode(old);
    check(legacy.getIntAttribute("version")==2&&output::decode(legacy,decoded)&&ampEq::flat(decoded.ampEq)&&!decoded.expandedAmpEq,"old output state remains byte-compatible and decodes flat");
    old.ampEq={3,-4,5};auto modern=output::encode(old);
    check(modern.getIntAttribute("version")==5&&output::decode(modern,decoded)&&decoded.ampEq.bassDb==3&&decoded.ampEq.midDb==-4&&decoded.ampEq.trebleDb==5,"v5 stores all three EQ values and existing effects");
    for(const String bad:{"nan","inf","12.1","","3garbage"}){auto broken=modern;broken.getChildByName("AmpEq")->setAttribute("bassDb",bad);auto sentinel=decoded;check(!output::decode(broken,sentinel)&&sentinel.ampEq.midDb==-4,"bad EQ rejected transactionally: "+bad);}
    auto duplicate=modern;duplicate.addChildElement(new XmlElement(*modern.getChildByName("AmpEq")));check(!output::decode(duplicate,decoded),"duplicate EQ node rejected");
    auto missing=modern;missing.removeChildElement(missing.getChildByName("AmpEq"),true);check(!output::decode(missing,decoded),"v5 requires explicit EQ node");
    auto flatNew=old;flatNew.ampEq={};flatNew.expandedAmpEq=true;auto reset=output::encode(flatNew);check(reset.getIntAttribute("version")==5&&output::decode(reset,decoded)&&ampEq::flat(decoded.ampEq),"explicit reset remains saved as neutral");
    board::State state;state.name="EQ rig";state.sourceType=amp::SourceType::Nam;state.namAmp.model={"Synthetic.nam",String::repeatedString("a",64),128};state.outputDsp=old;
    for(int p=0;p<board::count;++p)state.pedals[size_t(p)]=board::defaults(p,0);
    const board::RigIdentityLookup identity=[](int,int){return String::repeatedString("b",64);};PluginDescription description;board::State restored;
    auto rig=board::encode(state,description,identity);check(rig.getIntAttribute("version")==13&&board::decode(rig,description,identity,restored).wasOk()&&restored.outputDsp->ampEq.midDb==-4,"NAM rig v13 round trip retains EQ");
    state.sourceType=amp::SourceType::Vst3;state.genericVst3=true;description.name="Synthetic";description.pluginFormatName="VST3";description.fileOrIdentifier="Synthetic.vst3";description.uniqueId=123;description.version="test";state.amp.append("opaque",6);
    rig=board::encode(state,description,identity);check(board::decode(rig,description,identity,restored).wasOk()&&restored.outputDsp->ampEq.trebleDb==5,"VST3 rig retains dormant NAM EQ without changing opaque plugin state");
    for(int width:{1332,1632,2212})
    {juce::Rectangle<int> panel(14,500,width,ampui::compactHeight(width));const auto eq=ampui::eqBounds(panel);check(panel.contains(eq)&&eq.getHeight()/3>=26&&!eq.intersects(ampui::gainBounds(panel))&&!eq.intersects(ampui::volumeBounds(panel))&&!ampui::normalizationBounds(panel).intersects(ampui::gainBounds(panel)),"compact EQ has three usable non-overlapping rows / "+String(width));}
    report+=String(checks)+" EQ121 checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
