#pragma once
#include "Effects/Reverb/ReverbProcessor.h"
#include "Effects/Reverb/DattorroReverbCore.h"
#include <chrono>
namespace reverbBenchmark
{
inline bool run(juce::String& report)
{
    using namespace juce;using Clock=std::chrono::steady_clock;
    volatile double sink=0;
    auto measure=[&](const String& name,int block,auto&& process)
    {
        for(int i=0;i<1000;++i)process();
        std::array<double,5> trials{};
        for(auto& seconds:trials){const auto start=Clock::now();for(int i=0;i<5000;++i)process();seconds=std::chrono::duration<double>(Clock::now()-start).count();}
        std::sort(trials.begin(),trials.end());const double us=trials[2]*1.e6/5000;
        report+="CPU "+name+" rate=48000 block="+String(block)+" median_us="+String(us,3)+" realtime_percent="+String(us/(block/48000.*1.e6)*100,3)+"\n";
        return std::isfinite(us)&&us>0;
    };
    bool ok=true;
    for(int block:{96,512})
    {
        AudioBuffer<float> b(2,block);for(int c=0;c<2;++c)for(int i=0;i<block;++i)b.setSample(c,i,.04f*std::sin(i*.07f));
        tonegrid::ReverbProcessor dsp;dsp.prepare(48000,block);tonegrid::reverb::Parameters p;
        for(auto enabled:{false,true})
        {
            p.enabled=enabled;dsp.setTarget(p);
            ok&=measure(String("Dattorro")+(enabled?"_ON":"_OFF"),block,[&]{for(int i=0;i<block;++i){b.setSample(0,i,.04f*std::sin(i*.07f));b.setSample(1,i,.04f*std::sin(i*.07f));}dsp.process(b);sink+=b.getSample(0,block-1);});
        }
        // Isolate the very same prepared wet cores, excluding wrapper/control/input-fill cost.
        tonegrid::reverb::detail::DattorroCore core;core.prepare(48000);core.configure(1.5f,40,0,48000);
        ok&=measure("Dattorro_wet_core_only",block,[&]{for(int i=0;i<block;++i){DattorroVerb_process(&core.tank,.04*std::sin(i*.07));sink+=(DattorroVerb_getLeft(&core.tank)+DattorroVerb_getRight(&core.tank))*.35;}});
    }
    report+="CPU benchmark is a five-trial median on this PC, not hardware xrun/latency evidence. Dattorro and Spring drain their tails while OFF.\n";
    report+=String(ok?"PASS ":"FAIL ")+"Reverb CPU measurements complete and finite\n";return ok&&std::isfinite(sink);
}
}
