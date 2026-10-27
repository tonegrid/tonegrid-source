#pragma once
#include "Effects/Pitch/PitchEngine.h"
#include "EffectsNodes.h"
#include "EffectsControls.h"
#include "Shared/UI/ToneGridLogo.h"
#include "Content/Presets/RigStore.h"
#include "RealtimeAllocationProbe.h"
#include "Reference/OutputDsp115State.h"
#include <chrono>
#include <iostream>
namespace effects119Tests
{
using namespace juce;using namespace tonegrid;
inline bool finite(const std::vector<float>& x){return std::all_of(x.begin(),x.end(),[](float v){return std::isfinite(v)&&std::abs(v)<100;});}
inline std::vector<float> sine(double frequency,int count=24000)
{std::vector<float> x(count);for(int i=0;i<count;++i)x[i]=.1f*float(std::sin(MathConstants<double>::twoPi*frequency*i/48000));return x;}
inline void render(PitchEngine& engine,std::vector<float>& data)
{for(int i=0;i<int(data.size());i+=96)engine.process(data.data()+i,std::min(96,int(data.size())-i));}
inline double amplitude(const std::vector<float>& x,double hz,int from=4096)
{
    const double coefficient=2*std::cos(MathConstants<double>::twoPi*hz/48000);double a=0,b=0;
    for(int i=from;i<int(x.size());++i){const double v=x[i]+coefficient*a-b;b=a;a=v;}
    return std::sqrt(std::max(0.,a*a+b*b-coefficient*a*b))/(x.size()-from);
}
inline double frequency(const std::vector<float>& x,double expected)
{
    double best=0,hz=expected;const double width=std::max(4.,expected*.06);
    for(double f=std::max(1.,expected-width);f<=expected+width;f+=.5){const auto value=amplitude(x,f);if(value>best){best=value;hz=f;}}
    const auto coarse=hz;best=0;for(double f=coarse-.5;f<=coarse+.5;f+=.01){const auto value=amplitude(x,f);if(value>best){best=value;hz=f;}}return hz;
}
inline bool pitchStudy(String& report)
{
    bool ok=true;report+="PITCH STUDY 48000 Hz / 96 samples (wall CPU, one core)\n";
    for(auto config:{std::array<int,3>{256,64,256},std::array<int,3>{512,128,512},std::array<int,3>{1024,256,1024},std::array<int,3>{2048,512,2048},std::array<int,3>{4096,1024,4096},std::array<int,3>{2048,64,256},std::array<int,3>{2048,128,512},std::array<int,3>{2048,256,1024}})
    {
        const auto n=config[0],hop=config[1],synthesis=config[2];PitchEngine engine;engine.prepare(48000,n,hop,synthesis);
        bool stable=true;double maxCents=0,worstPurity=1;double seconds=0;
        for(double note:{82.4069,110.,220.,329.6276})for(int st:{-24,-12,12,24})
        {
            auto x=sine(note);engine.reset();engine.configure({pitch::Mode::Transpose,st});
            const auto start=std::chrono::steady_clock::now();render(engine,x);seconds+=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
            const double expected=note*std::pow(2.,st/12.),measured=frequency(x,expected);
            const double cents=1200*std::abs(std::log2(measured/expected));maxCents=std::max(maxCents,cents);stable&=finite(x);
            // Energy concentration at the desired fundamental, compared with RMS.
            double energy=0;for(size_t i=4096;i<x.size();++i)energy+=x[i]*x[i];const double rms=std::sqrt(energy/(x.size()-4096));
            const double purity=amplitude(x,measured)*std::sqrt(2.)/(rms+1e-12);worstPurity=std::min(worstPurity,purity);
            report+="FFT="+String(n)+" ST="+String(st)+" INPUT="+String(note,3)+" OUTPUT="+String(measured,3)+" CENTS="+String(cents,2)+" FUNDAMENTAL_RATIO="+String(purity,3)+"\n";
        }
        engine.reset();engine.configure({pitch::Mode::Octaver,0,100,50,50});auto chord=sine(82.4069);auto third=sine(103.8262);auto fifth=sine(123.4708);for(size_t i=0;i<chord.size();++i)chord[i]+=(third[i]+fifth[i])*.7f;render(engine,chord);stable&=finite(chord);
        engine.reset();engine.configure({pitch::Mode::Transpose,0});std::vector<float> warm(n*3,0);render(engine,warm);std::vector<float> impulse(n*2,0);impulse[0]=1;render(engine,impulse);const auto peak=int(std::max_element(impulse.begin(),impulse.end())-impulse.begin());
        report+="CONFIG FFT="+String(n)+" HOP="+String(hop)+" SYNTHESIS="+String(synthesis)+" LATENCY_SAMPLES="+String(peak)+" LATENCY_MS="+String(peak/48.,3)+" TRANSPOSE_CPU_PERCENT="+String(seconds/8.*100,3)+" MAX_CENTS="+String(maxCents,2)+" MIN_FUNDAMENTAL_RATIO="+String(worstPurity,3)+" STABLE="+String(stable?1:0)+"\n";
        report+="UNITY_ATTACK peak="+String(peak)+" expected="+String(engine.latencySamples())+" residual_energy=";
        double residual=0;for(int i=0;i<int(impulse.size());++i)if(i!=peak)residual+=impulse[i]*impulse[i];report+=String(residual,6)+"\n";
        ok&=stable&&peak==engine.latencySamples();
    }
    return ok;
}
inline bool run(String& report)
{
    bool ok=true;int checks=0;auto check=[&](bool value,const String& label){++checks;ok&=value;report+=String(value?"PASS ":"FAIL ")+label+"\n";};
    PitchEngine pitch;pitch.prepare(48000);auto dry=sine(110);auto expected=dry;render(pitch,dry);check(dry==expected,"Pitch OFF is bit-exact, zero added sample latency");
    for(int st:{-24,-12,12,24})
    {pitch.reset();pitch.configure({pitch::Mode::Transpose,st});auto x=sine(220);render(pitch,x);const double want=220*std::pow(2.,st/12.);const double got=frequency(x,want);check(finite(x)&&std::abs(1200*std::log2(got/want))<12,"Transpose "+String(st)+" pitch accuracy within 12 cents, finite non-silent signal");}
    for(auto levels:{std::array<float,3>{100,0,0},std::array<float,3>{0,100,0},std::array<float,3>{0,0,100},std::array<float,3>{100,50,50}})
    {
        pitch.reset();pitch.configure({pitch::Mode::Octaver,0,levels[0],levels[1],levels[2]});auto x=sine(220);render(pitch,x);
        check(finite(x)&&std::any_of(x.begin()+4096,x.end(),[](float v){return std::abs(v)>1e-4;}),"Octaver independently audible Dry/Down/Up/all: "+String(levels[0])+"/"+String(levels[1])+"/"+String(levels[2]));
        if(levels[0]==100&&levels[1]==0){auto source=sine(220);bool same=true;for(size_t i=4096;i<x.size();++i)same&=x[i]==source[i-pitch.latencySamples()];check(same,"Octaver Dry-only uses exact N-sample compensation, no immediate dry branch");}
    }
    pitch.reset();pitch.configure({pitch::Mode::Octaver,0,100,100,100});auto silence=std::vector<float>(12000,0);render(pitch,silence);check(std::all_of(silence.begin(),silence.end(),[](float x){return x==0;}),"Pitch silence remains exact silence with all octave voices");
    auto chord=sine(82.4069);for(int i=0;i<int(chord.size());++i){chord[i]+=.08f*float(std::sin(MathConstants<double>::twoPi*123.4708*i/48000));if(i%1000==0)chord[i]+=1;}render(pitch,chord);check(finite(chord),"Polyphonic low guitar chord and strong attacks stay finite");
    std::array<float,96> samples{};bool pitchSafe=true;size_t allocations=0;
    for(int i=0;i<1000;++i)
    {
        samples.fill(.03f);pitch::Parameters p{pitch::Mode(i%3),i%49-24,float(i%101),50,50};
        realtimeProbe::begin();pitch.configure(p);pitch.process(samples.data(),96);allocations+=realtimeProbe::end();pitchSafe&=std::all_of(samples.begin(),samples.end(),[](float v){return std::isfinite(v);});
    }
    check(allocations==0&&pitchSafe,"1000 rapid Pitch mode/transpose/level changes perform zero C++ allocations and stay finite");
    pitch.reset();float previous=0,maxJump=0;for(int k=0;k<600;++k){if(k%100==0)pitch.configure({pitch::Mode((k/100)%3),12});for(int i=0;i<96;++i)samples[i]=.1f*float(std::sin((k*96+i)*.03));pitch.process(samples.data(),96);for(float x:samples){maxJump=std::max(maxJump,std::abs(x-previous));previous=x;}}
    check(maxJump<.05f,"Pitch mode transitions bound sample discontinuity below half input amplitude on continuous sine");
    for(int i=0;i<5;++i){pitch.prepare(48000);pitch.reset();}check(pitch.latencySamples()==256,"Repeated Pitch prepare/reset retains selected configuration");
    pitch.configure({pitch::Mode::Transpose,0});std::vector<float> warm(8192,0);render(pitch,warm);std::vector<float> impulse(8192,0);impulse[0]=1;render(pitch,impulse);
    const auto peak=int(std::max_element(impulse.begin(),impulse.end())-impulse.begin());
    check(peak==pitch.latencySamples()&&finite(impulse),"Unity spectral impulse peak matches advertised 256-sample Octaver dry compensation");
    for(double note:{82.4069,110.,329.6276})for(int st:{-24,-12,12,24})
    {pitch.reset();pitch.configure({pitch::Mode::Transpose,st});auto x=sine(note);render(pitch,x);const auto want=note*std::pow(2.,st/12.);check(finite(x)&&std::abs(1200*std::log2(frequency(x,want)/want))<5,"Selected low-latency Pitch guitar fundamental "+String(note,1)+" Hz / "+String(st)+" st within 5 cents");}
    PitchEngine partitioned,whole;partitioned.prepare(48000);whole.prepare(48000);partitioned.configure({pitch::Mode::Octaver,0,100,50,30});whole.configure({pitch::Mode::Octaver,0,100,50,30});
    auto a=sine(110),b=a;render(partitioned,a);whole.process(b.data(),int(b.size()));check(a==b,"Pitch output is invariant to audio callback partitioning");
    AudioBuffer<float> block(2,96);ModProcessor mod;mod.prepare(48000,96);block.clear();mod.setTarget({});mod.process(block);check(block.getSample(0,0)==0,"MOD default OFF preserves silence");
    for(auto type:{mod::Type::Chorus,mod::Type::Tremolo,mod::Type::Phaser,mod::Type::Flanger})
    {
        mod.reset();mod.setTarget(mod::voicing(type));for(int i=0;i<96;++i){block.setSample(0,i,.1f);block.setSample(1,i,-.2f);}auto original=block;mod.process(block);bool identical=true;for(int c=0;c<2;++c)for(int i=0;i<96;++i)identical&=block.getSample(c,i)==original.getSample(c,i);check(identical,"MOD "+String(int(type))+" settled OFF preserves both stereo channels exactly");
        bool safe=true;allocations=0;double difference=0;
        for(int k=0;k<800;++k)
        {
            auto p=mod::voicing(type);p.enabled=k<760;p.rateHz=k%2?.05f:10.f;p.depth=k%3?100.f:0.f;p.level=k%5?100.f:0.f;
            for(int c=0;c<2;++c)for(int i=0;i<96;++i)block.setSample(c,i,k<600?.05f*float(std::sin((k*96+i)*.071+c)) :0.f);
            realtimeProbe::begin();mod.setTarget(p);mod.process(block);allocations+=realtimeProbe::end();
            for(int c=0;c<2;++c)for(int i=0;i<96;++i){safe&=std::isfinite(block.getSample(c,i))&&std::abs(block.getSample(c,i))<10;if(k<600)difference+=std::abs(block.getSample(c,i)-.05f*std::sin((k*96+i)*.071+c));}
        }
        check(safe&&allocations==0&&difference>1e-3,"MOD "+String(int(type))+" enable/bypass, Rate/Depth/Level extremes, silence: audible, finite, zero allocations");
    }
    allocations=0;for(int k=0;k<400;++k){auto p=mod::voicing(mod::Type(k%4));p.enabled=true;block.clear();realtimeProbe::begin();mod.setTarget(p);mod.process(block);allocations+=realtimeProbe::end();}check(allocations==0,"Rapid MOD type switching never allocates or constructs engines");
    DigitalDelay delay;delay.prepare(48000,96);for(int i=0;i<96;++i){block.setSample(0,i,.1f);block.setSample(1,i,-.2f);}auto original=block;delay.process(block);check(block.getSample(0,0)==original.getSample(0,0)&&block.getSample(1,95)==original.getSample(1,95),"Delay OFF preserves independent stereo channels exactly");
    for(double ms:{1.,100.,320.,2000.})
    {
        delay.setTarget({true,float(ms),50,100});delay.prepare(48000,96);for(int k=0;k<100;++k){block.clear();delay.process(block);}
        int time=int(ms*48);std::vector<float> echo(2*time+256);for(int k=0;k<int(echo.size());k+=96){block.clear();block.setSize(2,std::min(96,int(echo.size())-k),false,false,true);if(k==0)block.setSample(0,0,1);delay.process(block);std::copy(block.getReadPointer(0),block.getReadPointer(0)+block.getNumSamples(),echo.begin()+k);}block.setSize(2,96,false,false,true);
        check(std::abs(echo[time]-1)<1e-5&&std::abs(echo[2*time]-.5f)<1e-5&&echo[0]==0,"Digital Delay exact impulse time "+String(ms)+" ms, 50% feedback, fully wet with no immediate dry");
    }
    delay.setTarget({true,320,95,0});delay.prepare(48000,96);for(int k=0;k<100;++k){block.clear();delay.process(block);}block=original;delay.process(block);check(block.getSample(0,30)==original.getSample(0,30)&&block.getSample(1,30)==original.getSample(1,30),"Delay Mix 0 remains bit-exact while processing feedback");
    bool delaySafe=true;allocations=0;
    for(int k=0;k<6000;++k){block.clear();if(k%32==0)block.setSample(0,0,.5f);delay::Parameters p{k%20!=0,k%2?1.f:2000.f,k%3?95.f:0.f,float(k%101)};realtimeProbe::begin();delay.setTarget(p);delay.process(block);allocations+=realtimeProbe::end();for(int c=0;c<2;++c)for(int i=0;i<96;++i)delaySafe&=std::isfinite(block.getSample(c,i))&&std::abs(block.getSample(c,i))<20;}
    check(delaySafe&&allocations==0,"6000 delay blocks (12 seconds): time/feedback/mix/toggle extremes stay finite, zero allocations");
    output::ModNode modNode;output::DelayNode delayNode;modNode.prepareToPlay(48000,96);delayNode.prepareToPlay(48000,96);MidiBuffer nodeMidi;allocations=0;
    for(int k=0;k<400;++k){block.clear();auto mp=mod::voicing(mod::Type(k%4));mp.enabled=true;realtimeProbe::begin();modNode.configure(mp);delayNode.configure({true,float(k%2000+1),95,50});modNode.processBlock(block,nodeMidi);delayNode.processBlock(block,nodeMidi);allocations+=realtimeProbe::end();}
    check(allocations==0,"Actual MOD/Delay graph nodes publish and process changing atomic targets without C++ allocations");
    AudioBuffer<float> mono(1,96);bool repeated=true;for(double sr:{44100.,48000.,96000.}){mod.prepare(sr,96);delay.prepare(sr,96);mod.setTarget({true,mod::Type::Phaser,10,100,100});delay.setTarget({true,2000,95,100});for(int k=0;k<50;++k){mono.clear();mono.setSample(0,0,.1f);mod.process(mono);delay.process(mono);for(int i=0;i<96;++i)repeated&=std::isfinite(mono.getSample(0,i));}mod.reset();delay.reset();}
    check(repeated,"MOD/Delay repeated prepare/reset at 44.1/48/96 kHz handles mono safely");
    TapTime tap;check(tap.tap(0)==0&&tap.tap(500)==500&&tap.tap(1010)==505&&tap.tap(1020)==0&&tap.tap(1520)==500,"Tap averages intervals and rejects accidental short tap");
    check(tap.tap(6000)==0&&tap.tap(6500)==500,"Tap restarts after a long pause without setting an invalid time");
    output::Settings settings;settings.expandedEffects=true;settings.pitch={pitch::Mode::Octaver,-7,74,41,22};settings.mod={true,mod::Type::Flanger,3.2f,63,44};settings.delay={true,789.5f,71,29};settings.reverb.type=reverb::Type::Spring;
    auto xml=output::encode(settings);output::Settings decoded;check(output::decode(xml,decoded)&&output::encode(decoded).isEquivalentTo(&xml,false),"New OutputDSP v4 round-trips every Pitch/MOD/Delay field and Spring");
    auto implicit=settings;implicit.expandedEffects=false;auto implicitXml=output::encode(implicit);
    check(implicitXml.isEquivalentTo(&xml,false),"Changed new effect fields force expanded persistence even when caller omits opt-in flag");
    bool rejected=true;for(auto* tag:{"Pitch","Mod","Delay"}){auto bad=xml;bad.removeChildElement(bad.getChildByName(tag),true);auto kept=decoded;rejected&=!output::decode(bad,kept)&&output::encode(kept).isEquivalentTo(&xml,false);bad=xml;bad.addChildElement(new XmlElement(*bad.getChildByName(tag)));rejected&=!output::decode(bad,kept);}
    check(rejected,"Missing/duplicate effect nodes reject transactionally without partial publication");
    rejected=true;for(auto* badValue:{"nan","inf","5000","bad","-1"}){auto bad=xml;bad.getChildByName("Delay")->setAttribute("timeMs",badValue);rejected&=!output::decode(bad,decoded);}check(rejected,"Malformed/nonfinite/out-of-range new persisted values reject");
    for(int version:{1,2,3}){output::Settings old;if(version==3)old.reverb.type=reverb::Type::Spring;auto legacy=output::encode(old);if(version==1){legacy.setAttribute("version",1);legacy.removeChildElement(legacy.getChildByName("Reverb"),true);}check(output::decode(legacy,decoded)&&decoded.pitch.mode==pitch::Mode::Off&&!decoded.mod.enabled&&!decoded.delay.enabled&&!decoded.expandedEffects,"OutputDSP v"+String(version)+" migrates with Pitch/MOD/Delay OFF");}
    EffectsControls pc(EffectsPanel::Pitch),mc(EffectsPanel::Mod),dc(EffectsPanel::Delay);pc.sync(settings.pitch);mc.sync(settings.mod);dc.sync(settings.delay);
    check(pc.pitchSettings().dry==74&&mc.modSettings().type==mod::Type::Flanger&&dc.delaySettings().timeMs==789.5f,"All effect popups synchronize persisted controls");
    dc.applyTap(0);dc.applyTap(600);check(dc.delaySettings().timeMs==600,"Actual TAP popup control changes TIME on the message thread");
    bool defaults=true;auto* types=dynamic_cast<ComboBox*>(mc.findChildWithID("mod-type"));for(int i=1;i<=4;++i){types->setSelectedId(i,sendNotificationSync);auto got=mc.modSettings();auto wanted=mod::voicing(mod::Type(i-1));defaults&=got.rateHz==wanted.rateHz&&got.depth==wanted.depth&&got.level==wanted.level;}check(defaults,"Selecting each MOD type supplies its conservative guitar voicing");
    Logo logo;logo.setSize(180,80);auto logoImage=logo.createComponentSnapshot(logo.getLocalBounds());int white=0,transparent=0;for(int y=0;y<80;++y)for(int x=0;x<180;++x){auto pixel=logoImage.getPixelAt(x,y);white+=pixel.getAlpha()>0&&pixel.getRed()==255;transparent+=pixel.getAlpha()==0;}check(logo.valid()&&white>50&&transparent>50,"Embedded authoritative SVG draws white, transparent, sharp vector header logo");
    const auto folder=File::getSpecialLocation(File::currentExecutableFile).getSiblingFile("Effects119Preview");folder.createDirectory();
    for(auto pair:{std::pair<Component*,String>{&pc,"pitch"},{&mc,"mod"},{&dc,"delay"},{&logo,"logo"}}){auto stream=folder.getChildFile(pair.second+".png").createOutputStream();if(stream)PNGImageFormat().writeImageToStream(pair.first->createComponentSnapshot(pair.first->getLocalBounds()),*stream);}
    report+=String(checks)+" effects 1.1.9 checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
