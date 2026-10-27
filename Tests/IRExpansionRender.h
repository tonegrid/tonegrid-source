#pragma once
#include "NamAmp.h"
#include "OutputDspNodes.h"
namespace irExpansionRender
{
inline bool run(const juce::File& manifest,const juce::File& di,const juce::File& output,juce::String& report)
{
    using namespace juce;bool ok=true;
    try {
        const auto json=nlohmann::json::parse(manifest.loadFileAsString().toStdString());
        MemoryBlock input;if(!di.loadFileAsData(input)||input.getSize()%sizeof(float)||!input.getSize())return false;
        if(output.createDirectory().failed())return false;const auto count=input.getSize()/sizeof(float);const auto* samples=static_cast<const float*>(input.getData());
        for(const auto& record:json.at("captures")) {
            const String id(record.at("id").get<std::string>());const auto destination=output.getChildFile(id+".f32");
            if(destination.getSize()==juce::int64(input.getSize()))continue;
            const File file(String(record.at("file").get<std::string>()));namamp::Library library(file.getParentDirectory());namamp::Entry entry;
            auto result=library.read(file,entry);if(result.failed()){report+="FAIL "+id+": "+result.getErrorMessage()+"\n";ok=false;continue;}
            auto loaded=amp::loadNamAmp(library,{entry.identity,0,0},false);auto* source=dynamic_cast<amp::NamAmp*>(loaded.source.get());
            if(!source||loaded.result.failed()){report+="FAIL "+id+": "+loaded.result.getErrorMessage()+"\n";ok=false;continue;}
            source->setCalibration(tonegrid::calibration::Settings{true,12.5f});AudioProcessorGraph graph;auto node=source->addToGraph(graph);if(!node){ok=false;continue;}
            auto* processor=node->getProcessor();processor->prepareToPlay(48000,128);
            const float base=tonegrid::AmpNormalizationProcessor::adjustmentDb(source->loudnessDb());const float gain=Decibels::decibelsToGain(base);
            AudioBuffer<float> audio(2,128);MidiBuffer midi;MemoryBlock rendered(count*sizeof(float),true);auto* raw=static_cast<float*>(rendered.getData());bool finite=true;
            for(size_t position=0;position<count;position+=128) {
                const auto n=int(std::min(size_t(128),count-position));audio.setSize(2,n,false,false,true);
                for(int i=0;i<n;++i)for(int c=0;c<2;++c)audio.setSample(c,i,samples[position+size_t(i)]);
                processor->processBlock(audio,midi);
                for(int i=0;i<n;++i){const float value=audio.getSample(0,i)*gain;finite&=std::isfinite(value);raw[position+size_t(i)]=value;}
            }
            finite&=!source->takeFault();if(!finite||!destination.replaceWithData(rendered.getData(),rendered.getSize()))ok=false;
            report+=(finite?"PASS ":"FAIL ")+id+"\n";
        }
    }catch(const std::exception& e){report+=String(e.what());return false;}return ok;
}
}
