// SPDX-License-Identifier: MIT
// Tests adapted in part from TONE3000 spread_tests.cpp.
// Copyright (c) 2026 TONE3000. See Resources/Licenses/TONE3000-MIT.txt.
#pragma once
#include "Effects/StereoSpread/StereoSpreadProcessor.h"
#include "OutputDspParameters.h"
#include <vector>
namespace spreadTests
{
inline bool run(juce::String& report)
{
    using namespace juce;using tonegrid::SpreadParams;using tonegrid::StereoSpreadProcessor;
    bool ok=true;int checks=0;
    auto check=[&](bool good,const String& name){++checks;ok&=good;report+=String(good?"PASS ":"FAIL ")+name+"\n";};
    struct Render {std::vector<float> left,right;};
    auto render=[](SpreadParams p,double frequency,int blocks=256,double rate=48000.){
        StereoSpreadProcessor processor;processor.prepare(rate,256);processor.setTarget(p,true);
        AudioBuffer<float> audio(2,256);Render result;result.left.reserve((size_t)blocks*256);result.right.reserve((size_t)blocks*256);
        for(int block=0;block<blocks;++block){for(int i=0;i<256;++i){const auto x=.5f*(float)std::sin(MathConstants<double>::twoPi*frequency*(block*256+i)/rate);audio.setSample(0,i,x);audio.setSample(1,i,x);}processor.process(audio);for(int i=0;i<256;++i){result.left.push_back(audio.getSample(0,i));result.right.push_back(audio.getSample(1,i));}}
        return result;
    };
    auto differenceDb=[](const Render& r){double difference=0,energy=0;for(size_t i=24000;i<r.left.size();++i){difference+=std::pow(r.left[i]-r.right[i],2);energy+=r.left[i]*r.left[i];}return 10*std::log10((difference+1e-30)/(energy+1e-30));};
    SpreadParams params;params.wobbleDepth=0;
    check(params.offsetMs==15&&params.crossoverHz==130&&params.crossoverOn&&params.diffuseOn,"Spread defaults match classic +15 ms ADT with 130 Hz crossover");
    tonegrid::output::Settings settings;
    check(settings.wobblePercent==25&&settings.wobbleEnabled&&!settings.spreadEnabled&&!settings.ampNormalizationEnabled,"Spread initially bypassed; Wobble 25 percent and deck switches on; NAM normalization OFF by default");
    const auto bad=SpreadParams::sanitized({INFINITY,NAN,-5,true,true});
    check(bad.offsetMs==15&&bad.wobbleDepth==.25f&&bad.crossoverHz==32.5f,"Spread invalid numbers fall back safely and crossover clips to bounds");
    check(SpreadParams::sanitized({-99,5,900,true,true}).offsetMs==-24&&SpreadParams::sanitized({99,-5,900,true,true}).offsetMs==24&&SpreadParams::sanitized({99,-5,900,true,true}).crossoverHz==520,"Spread offset/depth/crossover parameter bounds enforced");
    check(SpreadParams::fromNormalized(.5f,.25f,.5f,true,true,true).offsetMs==0&&SpreadParams::fromNormalized(.5f,.25f,.5f,true,true,true).crossoverHz==130,"Spread normalized center and log crossover match reference");
    StereoSpreadProcessor idle;idle.prepare(48000,256);AudioBuffer<float> audio(2,256);
    for(int i=0;i<256;++i){audio.setSample(0,i,i%2?-.0f:.1f*i);audio.setSample(1,i,-.3f*i);}
    AudioBuffer<float> original;original.makeCopyOf(audio);idle.setTarget(params,false);idle.process(audio);
    check(std::memcmp(audio.getReadPointer(0),original.getReadPointer(0),256*sizeof(float))==0&&std::memcmp(audio.getReadPointer(1),original.getReadPointer(1),256*sizeof(float))==0,"Spread settled bypass preserves distinct L/R and signed zero bit-exactly");
    params.offsetMs=0;const auto center=render(params,440);
    check(center.left==center.right,"Spread zero offset is exactly dual mono even with crossover and diffusion on");
    params.offsetMs=12;const auto positive=render(params,431);params.offsetMs=-12;const auto negative=render(params,431);
    check(positive.left==negative.right&&positive.right==negative.left,"Spread signed offset mirrors the lagged channel exactly with zero wobble");
    check(differenceDb(positive)>-10,"Spread creates a nontrivial stereo image from mono high-frequency input");
    params.offsetMs=12;const auto low=render(params,30);
    check(differenceDb(low)<-35,"Spread 30 Hz stays effectively dual mono below 130 Hz crossover");
    params.crossoverOn=false;check(differenceDb(render(params,30))>-10,"Spread crossover off applies doubling to the whole band");
    params.crossoverOn=true;params.crossoverHz=520;const auto lowCut=render(params,250);params.crossoverHz=32.5f;
    check(differenceDb(lowCut)<-15&&differenceDb(render(params,250))>-10,"Spread LR4 cutoff moves from 32.5 to 520 Hz with expected low/high separation");
    params.crossoverOn=false;params.diffuseOn=false;params.crossoverHz=130;
    StereoSpreadProcessor delay;delay.prepare(48000,256);delay.setTarget(params,true);bool pure=true;
    const float lagGain=Decibels::decibelsToGain(1.5f);
    for(int block=0;block<100;++block){for(int i=0;i<256;++i){const auto x=.5f*(float)std::sin((block*256+i)*.03);audio.setSample(0,i,x);audio.setSample(1,i,x);}delay.process(audio);for(int i=0;i<256;++i){const auto frame=block*256+i;if(frame>4800){pure&=audio.getSample(0,i)==.5f*(float)std::sin(frame*.03);pure&=std::abs(audio.getSample(1,i)-lagGain*.5f*(float)std::sin((frame-576)*.03))<1e-5;}}}
    check(pure,"Spread sections off leave exact dry side and +1.5 dB pure 12 ms lag");
    params.offsetMs=576.5f/48.f;const auto fractional=render(params,431);
    bool interpolated=true;
    for(size_t i=24000;i<fractional.right.size();++i){auto sample=[&](int age){return .5f*(float)std::sin(MathConstants<double>::twoPi*431*((double)i-age)/48000.);};const auto expected=lagGain*(-.0625f*sample(575)+.5625f*sample(576)+.5625f*sample(577)-.0625f*sample(578));interpolated&=std::abs(fractional.right[i]-expected)<2e-5;}
    check(interpolated,"Spread half-sample delay matches independent four-point Lagrange weights");
    params.offsetMs=12;params.diffuseOn=true;const auto diffused=render(params,375);
    double dryEnergy=0,lagEnergy=0;bool periodic=true;
    for(size_t i=24000;i<diffused.left.size();++i){dryEnergy+=diffused.left[i]*diffused.left[i];lagEnergy+=diffused.right[i]*diffused.right[i];periodic&=std::abs(diffused.right[i]-diffused.right[i-128])<2e-6;}
    check(std::abs(10*std::log10(lagEnergy/dryEnergy)-1.5)<.01,"Six-stage diffusion remains magnitude-flat apart from required +1.5 dB lag compensation");
    check(periodic&&SpreadParams::fromNormalized(.75f,1,.5f,false,true,true).wobbleDepth==0,"Wobble switch off maps to zero depth and leaves lag time invariant");
    params.offsetMs=0;params.wobbleDepth=1;const auto wobblingCenter=render(params,431);
    check(wobblingCenter.left==wobblingCenter.right,"Zero offset stays exactly dual mono even at full random-walk depth");
    params={20,1,130,true,true};const auto drifting=render(params,500,576);
    auto power=[&](double frequency){double real=0,imaginary=0;const size_t start=48000,n=drifting.right.size()-start;for(size_t i=0;i<n;++i){const auto window=.5-.5*std::cos(MathConstants<double>::twoPi*i/(n-1));const auto phase=MathConstants<double>::twoPi*frequency*i/48000.;const auto value=drifting.right[start+i]*window;real+=value*std::cos(phase);imaginary+=value*std::sin(phase);}return real*real+imaginary*imaginary;};
    const auto carrier=power(500);bool noFizz=true;for(double frequency:{2917.,4111.,6373.,9241.,13687.})noFizz&=10*std::log10((power(frequency)+1e-30)/(carrier+1e-30))< -90;
    check(noFizz,"Two-pole slow random-walk wobble keeps remote spectral bins below -90 dB relative to 500 Hz carrier");
    StereoSpreadProcessor toggles;toggles.prepare(48000,256);params={15,.25f,130,true,true};toggles.setTarget(params,true);audio.clear();audio.applyGain(1);
    for(int i=0;i<256;++i){audio.setSample(0,i,.2f);audio.setSample(1,i,.2f);}toggles.process(audio);
    check(std::abs(audio.getSample(1,0)-.2f)<.002f&&toggles.isRunning(),"Spread engage begins with a small 25 ms crossfade step");
    bool smooth=true;float previousL=.2f,previousR=.2f;
    for(int block=0;block<140;++block){params.offsetMs=block%40<20?24:-24;params.crossoverOn=block%30<15;params.diffuseOn=block%26<13;toggles.setTarget(params,block<120);for(int i=0;i<256;++i){const auto x=.2f*(float)std::sin((block*256+i)*.05);audio.setSample(0,i,x);audio.setSample(1,i,x);}toggles.process(audio);for(int i=0;i<256;++i){const auto l=audio.getSample(0,i),r=audio.getSample(1,i);if(block>10)smooth&=std::isfinite(l)&&std::isfinite(r)&&std::abs(l-previousL)<.08f&&std::abs(r-previousR)<.08f;previousL=l;previousR=r;}}
    check(smooth,"Spread extreme offset sign changes and crossover/diffuse toggles remain smooth");
    check(!toggles.isRunning(),"Spread bypass reaches fully idle after 25 ms fade");
    original.makeCopyOf(audio);toggles.process(audio);check(std::memcmp(original.getReadPointer(0),audio.getReadPointer(0),256*sizeof(float))==0&&std::memcmp(original.getReadPointer(1),audio.getReadPointer(1),256*sizeof(float))==0,"Spread bypass after engage/disable preserves both original channels exactly");
    AudioBuffer<float> mono(1,256);for(int i=0;i<256;++i)mono.setSample(0,i,.1f*i);AudioBuffer<float> monoOriginal;monoOriginal.makeCopyOf(mono);toggles.setTarget(params,true);toggles.process(mono);
    check(!toggles.isRunning()&&std::memcmp(mono.getReadPointer(0),monoOriginal.getReadPointer(0),256*sizeof(float))==0,"Spread stays inactive and transparent on true mono output");
    bool portable=true;for(const double rate:{8000.,44100.,48000.,96000.,192000.}){const auto data=render(params,431,16,rate);for(auto value:data.right)portable&=std::isfinite(value);}check(portable,"Spread processes safely at multiple sample rates without platform-specific DSP");
    AudioBuffer<float> empty(2,0);toggles.setTarget(params,true);toggles.process(empty);check(std::isfinite(toggles.correlation()),"Spread empty blocks do not generate invalid correlation state");
    report+=String(checks)+" stereo spread checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
