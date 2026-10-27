#pragma once
#include "Effects/Reverb/ReverbProcessor.h"
#include "Effects/Reverb/SpringReverbProcessor.h"
#include <chrono>
namespace springBenchmark
{
inline bool run(juce::String& report)
{
    using namespace juce;using Clock=std::chrono::steady_clock;bool ok=true;volatile double sink=0;
    auto measure=[&](const String& name,int block,auto&& process)
    {
        for(int i=0;i<500;++i)process();std::array<double,5> trials{};
        for(auto& seconds:trials){const auto start=Clock::now();for(int i=0;i<2000;++i)process();seconds=std::chrono::duration<double>(Clock::now()-start).count();}
        std::sort(trials.begin(),trials.end());const double us=trials[2]*1.e6/2000;
        report+="CPU "+name+" rate=48000 block="+String(block)+" median_us="+String(us,3)+" realtime_percent="+String(us/(block/48000.*1.e6)*100,3)+"\n";ok&=std::isfinite(us)&&us>0;
    };
    for(int block:{96,512})
    {
        AudioBuffer<float> input(2,block),b(2,block);for(int i=0;i<block;++i){input.setSample(0,i,.04f*std::sin(i*.07f));input.setSample(1,i,.03f*std::cos(i*.03f));}
        for(int mode=-1;mode<4;++mode){tonegrid::ReverbProcessor dsp;tonegrid::reverb::Parameters p;p.enabled=mode>=0;p.type=mode<0?tonegrid::reverb::Type::Room:tonegrid::reverb::Type(mode);dsp.setTarget(p);dsp.prepare(48000,block);
            measure(mode<0?"OFF":StringArray{"Room","Plate","Large","Spring"}[mode],block,[&]{for(int c=0;c<2;++c)b.copyFrom(c,0,input,c,0,block);dsp.process(b);sink+=b.getSample(0,block-1);});}
        tonegrid::SpringReverbProcessor core;core.prepare(48000,block);measure("Spring_wet_core_only",block,[&]{for(int i=0;i<block;++i){auto wet=core.processSample(input.getSample(0,i),input.getSample(1,i));sink+=wet[0]+wet[1];}});
    }
    report+="Five-trial median on this Windows x64 PC, without physical ASIO/xrun/latency testing. Combined wrapper clocks all prepared wet engines, including OFF, for draining tails and warmed type transitions.\n";
    report+=String(ok?"PASS ":"FAIL ")+"Spring CPU measurements finite\n";return ok&&std::isfinite(sink);
}
}
