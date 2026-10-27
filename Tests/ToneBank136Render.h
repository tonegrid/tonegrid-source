#pragma once
#include "Content/Presets/ToneBank.h"
#include "OutputDspNodes.h"
namespace toneBank136Render
{
// A measured guitar DI, independent instances per preset, no devices or user data.
inline bool run(const juce::File& payload,const juce::File& di,const juce::File& evidence,juce::String& report)
{
    using namespace juce;using namespace tonegrid;
    try
    {
        MemoryBlock input;if(!di.loadFileAsData(input)||input.getSize()==0||input.getSize()%sizeof(float))return false;
        if(evidence.createDirectory().failed())return false;
        const auto samples=input.getSize()/sizeof(float);const auto* raw=static_cast<const float*>(input.getData());
        tonebank::Bank bank;if(tonebank::parse(payload.getChildFile("ToneBank/tonebank.json").loadFileAsString(),bank).failed())return false;
        namamp::Library library(payload.getChildFile("FactoryAmps"));namamp::Catalog catalog;if(library.refresh(catalog).failed())return false;
        MemoryOutputStream csv;csv.writeText("rank,title,peak_dbfs,finite\n",false,false,nullptr);
        for(const auto& preset:bank.presets)
        {
            const bool selected=preset.rank==7||preset.rank==40||preset.rank==43||preset.rank==65||preset.rank==66||preset.rank==98||preset.rank==101;
            if(!selected)continue;
            board::Processor processor;const auto models=payload.getChildFile("Models");if(processor.load(models).failed())return false;
            auto plan=tonebank::prepare(preset,catalog,processor,models);if(!plan.replaceAmp||!plan.warnings.isEmpty())return false;
            auto loaded=amp::loadNamAmp(library,plan.state.namAmp);if(loaded.result.failed()||!loaded.source)return false;
            auto* native=dynamic_cast<amp::NamAmp*>(loaded.source.get());if(!native)return false;
            AudioProcessorGraph graph;auto ampNode=loaded.source->addToGraph(graph);if(!ampNode)return false;
            const calibration::Settings reference(true,12.5f);processor.setCalibration(reference);native->setCalibration(reference);native->setUpstreamReference(&processor.referenceRouting);
            for(int id=0;id<board::count;++id)processor.set(id,plan.state.pedals[size_t(id)]);
            processor.front.set(plan.state.input);processor.prepareToPlay(48000,128);ampNode->getProcessor()->prepareToPlay(48000,128);
            output::AmpEqNode eq;output::NormalizationNode normalization;output::SpreadNode spread;output::ReverbNode reverb;output::ModNode mod;output::DelayNode delay;
            for(auto* node:std::initializer_list<AudioProcessor*>{&normalization,&eq,&spread,&reverb,&mod,&delay})node->prepareToPlay(48000,128);
            const auto& fx=*plan.state.outputDsp;eq.configure(fx.ampEq,true);normalization.configure(fx.ampNormalizationEnabled,true,native->loudnessDb(),output::selectedNormalizationTrim(fx,native->normalizationTrimDb()));spread.configure(fx);mod.configure(fx.mod);delay.configure(fx.delay);reverb.configure(fx.reverb);processor.pitch.configure(fx.pitch);
            MemoryBlock output(samples*2*sizeof(float));auto* rendered=static_cast<float*>(output.getData());AudioBuffer<float> audio(2,128);MidiBuffer midi;double peak=0;bool finite=true;
            for(size_t position=0;position<samples;position+=128)
            {
                const auto frames=int(std::min(size_t(128),samples-position));audio.setSize(2,frames,false,false,true);
                for(int i=0;i<frames;++i)for(int c=0;c<2;++c)audio.setSample(c,i,raw[position+size_t(i)]);
                processor.processBlock(audio,midi);ampNode->getProcessor()->processBlock(audio,midi);normalization.processBlock(audio,midi);eq.processBlock(audio,midi);spread.processBlock(audio,midi);mod.processBlock(audio,midi);delay.processBlock(audio,midi);reverb.processBlock(audio,midi);
                for(int i=0;i<frames;++i)for(int c=0;c<2;++c){const auto value=audio.getSample(c,i);finite&=std::isfinite(value);peak=jmax(peak,std::abs(double(value)));rendered[(position+size_t(i))*2+size_t(c)]=value;}
            }
            finite&=!processor.fault.load()&&!native->takeFault();if(!finite)return false;
            if(!evidence.getChildFile("rank-"+String(preset.rank).paddedLeft('0',3)+".f32").replaceWithData(output.getData(),output.getSize()))return false;
            csv.writeText(String(preset.rank)+",\""+preset.title()+"\","+String(Decibels::gainToDecibels(peak,-120.0),3)+",true\n",false,false,nullptr);
            report+="PASS real DI full preset render: "+preset.title()+" peak "+String(Decibels::gainToDecibels(peak,-120.0),3)+" dBFS\n";
            loaded.source->unload();
        }
        return evidence.getChildFile("levels.csv").replaceWithData(csv.getData(),csv.getDataSize());
    }
    catch(const std::exception& error){report+=String(error.what());return false;}
}
}
