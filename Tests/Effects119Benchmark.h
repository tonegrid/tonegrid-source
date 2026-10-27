#pragma once
#include "EffectsNodes.h"
#include "Effects/Pedalboard/PedalBoard.h"
#include "NamAmp.h"
#include <chrono>
namespace effects119Benchmark
{
inline bool run(const juce::File& models,const juce::File& ampFile,juce::String& report)
{
    using namespace juce;using namespace tonegrid;using Clock=std::chrono::steady_clock;
    if(!ampFile.existsAsFile()){report+="FAIL Real NAM benchmark source missing\n";return false;}
    namamp::Library library(ampFile.getParentDirectory());namamp::Catalog catalog;const auto scan=library.refresh(catalog);namamp::State state;bool found=false;
    for(const auto& e:catalog.entries)if(e.identity.file==ampFile.getFileName()){state.model=e.identity;found=true;break;}
    auto loaded=found?amp::loadNamAmp(library,state):amp::NamLoadResult{nullptr,Result::fail("NAM model not found")};
    if(!loaded.source){report+="FAIL NAM benchmark load: "+loaded.result.getErrorMessage()+"\n";return false;}
    rig::StereoGraph ownership;auto ampNode=loaded.source->addToGraph(ownership);auto* nam=ampNode->getProcessor();nam->prepareToPlay(48000,96);
    board::Processor pedals;if(pedals.load(models).failed())return false;for(int id=0;id<13;++id){auto p=pedals.get(id);p.enabled=false;pedals.set(id,p);}pedals.prepareToPlay(48000,96);
    AmpNormalizationProcessor normalize;normalize.prepare(48000);const auto* source=dynamic_cast<const amp::NamAmp*>(loaded.source.get());normalize.setTarget(source->loudnessDb(),true,true);
    StereoSpreadProcessor spread;spread.prepare(48000,96);
    ModProcessor modulation;modulation.prepare(48000,96);DigitalDelay delay;delay.prepare(48000,96);ReverbProcessor reverb;reverb.prepare(48000,96);
    AudioBuffer<float> input(2,96),audio(2,96);MidiBuffer midi;volatile double sink=0;bool ok=true;
    for(int i=0;i<96;++i){input.setSample(0,i,.035f*float(std::sin(i*.071)));input.setSample(1,i,input.getSample(0,i));}
    report+="CPU BENCHMARK 48000 Hz / 96 samples, real NAM="+ampFile.getFileName()+" SHA256="+SHA256(ampFile).toHexString()+"\n";
    const StringArray labels{"Baseline_118_equivalent","Baseline_plus_Transpose","Baseline_plus_Octaver_Dry_Down_Up","Baseline_plus_Chorus","Baseline_plus_Tremolo","Baseline_plus_Phaser","Baseline_plus_Flanger","Baseline_plus_DigitalDelay","Combined_Pitch_Chorus_Delay_Plate_NAM"};
    for(int scenario=0;scenario<labels.size();++scenario)
    {
        pitch::Parameters pitch;mod::Parameters mod;delay::Parameters echo;reverb::Parameters verb;
        if(scenario==1||scenario==8)pitch={pitch::Mode::Transpose,-2};
        if(scenario==2)pitch={pitch::Mode::Octaver,0,100,50,30};
        if(scenario>=3&&scenario<=6){mod=mod::voicing(mod::Type(scenario-3));mod.enabled=true;}
        if(scenario==8){mod=mod::voicing(mod::Type::Chorus);mod.enabled=true;verb.enabled=true;verb.type=reverb::Type::Plate;}
        if(scenario==7||scenario==8)echo.enabled=true;
        pedals.pitch.reset();pedals.pitch.configure(pitch);modulation.reset();modulation.setTarget(mod);delay.reset();delay.setTarget(echo);reverb.setTarget(verb);
        auto process=[&]{for(int c=0;c<2;++c)audio.copyFrom(c,0,input,c,0,96);pedals.processBlock(audio,midi);nam->processBlock(audio,midi);normalize.process(audio);spread.process(audio);if(scenario!=0){modulation.process(audio);delay.process(audio);}reverb.process(audio);sink+=audio.getSample(0,95);};
        for(int i=0;i<500;++i)process();std::array<double,5> times{};
        for(auto& seconds:times){const auto start=Clock::now();for(int i=0;i<2000;++i)process();seconds=std::chrono::duration<double>(Clock::now()-start).count();}
        std::sort(times.begin(),times.end());const auto us=times[2]*1e6/2000;
        report+="CPU "+labels[scenario]+" median_us="+String(us,3)+" realtime_percent="+String(us/2000*100,3)+"\n";ok&=std::isfinite(us)&&us>0;
    }
    report+="Five-trial median, complete processing chain including copies/gate/bypassed pedals/real NAM/normalization/Spread OFF/current Reverb. Baseline skips the new MOD/Delay calls and Pitch is OFF. Percent of one core's 2 ms callback budget, not process-wide CPU or Raspberry Pi/iPad prediction.\n";
    return ok&&std::isfinite(sink);
}
}
