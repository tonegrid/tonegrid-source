#pragma once
#include "RealtimeAllocationProbe.h"
#include "AmpNormalizationProcessor.h"
#include "Effects/StereoSpread/StereoSpreadProcessor.h"
#include "OutputDspNodes.h"
namespace outputRealtimeTests
{
inline bool run(juce::String& report)
{
    using namespace juce;bool ok=true;int checks=0;
    auto check=[&](bool good,const String& name){++checks;ok&=good;report+=String(good?"PASS ":"FAIL ")+name+"\n";};
    realtimeProbe::begin();auto* positive=::operator new(64);const auto observed=realtimeProbe::end();::operator delete(positive);
    check(observed==1,"RT allocation probe detects an intentional allocation (positive control)");
    tonegrid::AmpNormalizationProcessor normalize;normalize.prepare(48000);
    tonegrid::StereoSpreadProcessor spread;spread.prepare(48000,256);
    const std::array<int,9> sizes{1,3,95,96,128,256,511,512,1000};
    std::vector<AudioBuffer<float>> buffers;for(auto frames:sizes)buffers.emplace_back(2,frames);
    AudioBuffer<float> mono(1,256);
    bool finite=true;realtimeProbe::begin();
    for(int iteration=0;iteration<2000;++iteration)
    {
        auto& buffer=buffers[(size_t)iteration%buffers.size()];
        for(int i=0;i<buffer.getNumSamples();++i){const auto x=.03f*(float)std::sin(i*.031);buffer.setSample(0,i,x);buffer.setSample(1,i,-x);}
        normalize.setTarget(iteration%3?std::optional<float>{-24.f}:std::nullopt,iteration%5!=0,iteration%7!=0,iteration%2?14.56f:-7.66f);
        normalize.process(buffer);
        spread.setTarget({iteration%4<2?24.f:-24.f,iteration%2?1.f:0.f,iteration%2?520.f:32.5f,iteration%7!=0,iteration%11!=0},iteration%13!=0);
        spread.process(buffer);
        if(iteration%19==0){mono.clear();spread.process(mono);}
        for(int channel=0;channel<2;++channel)for(int i=0;i<buffer.getNumSamples();++i)finite&=std::isfinite(buffer.getSample(channel,i));
    }
    const auto count=realtimeProbe::end();
    check(count==0,"New DSP processing/parameter/engage/mono transitions allocate zero C++ objects after prepare across 2000 callbacks");
    check(finite,"RT parameter stress and odd/oversized blocks remain finite");
    tonegrid::output::NormalizationNode normNode;tonegrid::output::SpreadNode spreadNode;
    normNode.prepareToPlay(48000,256);spreadNode.prepareToPlay(48000,256);MidiBuffer midi;
    tonegrid::output::Settings settings;settings.spreadEnabled=true;finite=true;realtimeProbe::begin();
    for(int iteration=0;iteration<2000;++iteration)
    {
        auto& buffer=buffers[(size_t)iteration%buffers.size()];buffer.clear();
        normNode.configure(iteration%5!=0,iteration%7!=0,-24,iteration%2?14.56f:-7.66f);normNode.processBlock(buffer,midi);
        settings.offsetMs=iteration%2?24:-24;settings.crossoverHz=iteration%2?520:32.5f;settings.wobblePercent=iteration%2?100:0;settings.spreadEnabled=iteration%13!=0;
        spreadNode.configure(settings);spreadNode.stereoOutputAvailable(iteration%19!=0);spreadNode.processBlock(buffer,midi);
        for(int channel=0;channel<2;++channel)for(int i=0;i<buffer.getNumSamples();++i)finite&=std::isfinite(buffer.getSample(channel,i));
    }
    const auto nodeAllocations=realtimeProbe::end();
    check(nodeAllocations==0&&finite,"Graph DSP adapters and atomic control updates also allocate zero C++ objects after prepare across 2000 callbacks");
    report+=String(checks)+" output RT checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
