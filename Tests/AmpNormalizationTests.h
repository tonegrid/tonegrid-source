#pragma once
#include "AmpNormalizationProcessor.h"
#include "Content/Amps/NamAmpLibrary.h"
#include "NamAmp.h"
#include "OutputDspNodes.h"
namespace normalizationTests
{
// Offline evidence only: exercise the actual library -> NAM source -> output
// node path with the supplied DI. No audio devices or saved user state are used.
inline bool render(const juce::File& measurements,const juce::File& diFile,const juce::File& directory,juce::String& report)
{
    using namespace juce;
    try
    {
        const auto data=nlohmann::json::parse(measurements.loadFileAsString().toStdString());
        MemoryBlock input;if(!diFile.loadFileAsData(input)||input.getSize()==0||input.getSize()%sizeof(float)!=0)return false;
        if(directory.createDirectory().failed())return false;
        const auto count=input.getSize()/sizeof(float);const auto* samples=static_cast<const float*>(input.getData());
        for(const auto& capture:data.at("captures"))
        {
            const File file(String(capture.at("file").get<std::string>()));namamp::Library library(file.getParentDirectory());namamp::Entry entry;
            if(library.read(file,entry).failed()||std::abs(entry.normalizationTrimDb-capture.at("proposed_metadata").at("Tonegrid_Normalization").at("trim_db").get<float>())>1e-5f)return false;
            auto loaded=amp::loadNamAmp(library,{entry.identity,0,0},false);
            auto* source=dynamic_cast<amp::NamAmp*>(loaded.source.get());if(!source||loaded.result.failed())return false;
            source->setCalibration(tonegrid::calibration::Settings{true,12.5f});AudioProcessorGraph graph;
            auto node=source->addToGraph(graph);if(!node)return false;auto* processor=node->getProcessor();processor->prepareToPlay(48000,128);
            tonegrid::output::NormalizationNode normalization;normalization.prepareToPlay(48000,128);
            normalization.configure(true,true,source->loudnessDb(),source->normalizationTrimDb());
            AudioBuffer<float> audio(2,128);MidiBuffer midi;MemoryBlock output(count*sizeof(float),true);auto* rendered=static_cast<float*>(output.getData());
            for(size_t position=0;position<count;position+=128)
            {
                const auto frames=static_cast<int>(std::min(size_t(128),count-position));audio.setSize(2,frames,false,false,true);
                for(int i=0;i<frames;++i){audio.setSample(0,i,samples[position+size_t(i)]);audio.setSample(1,i,samples[position+size_t(i)]);}
                processor->processBlock(audio,midi);normalization.processBlock(audio,midi);
                for(int i=0;i<frames;++i){const auto value=audio.getSample(0,i);if(!std::isfinite(value))return false;rendered[position+size_t(i)]=value;}
            }
            const auto name=String(capture.at("id").get<std::string>());
            if(!directory.getChildFile(name+"-app125.f32").replaceWithData(output.getData(),output.getSize()))return false;
            report+="PASS actual ToneGrid NAM source and normalization node render: "+name+"\n";
        }
        return true;
    }
    catch(const std::exception& e){report+=String(e.what());return false;}
}
inline bool run(juce::String& report)
{
    using namespace juce;using tonegrid::AmpNormalizationProcessor;
    bool ok=true;int checks=0;
    auto check=[&](bool good,const String& name){++checks;ok&=good;report+=String(good?"PASS ":"FAIL ")+name+"\n";};
    check(AmpNormalizationProcessor::adjustmentDb(-24)==6,"Normalization metadata -24 dB -> static +6 dB");
    check(AmpNormalizationProcessor::adjustmentDb({})==0,"Normalization absent metadata -> unity");
    check(AmpNormalizationProcessor::adjustmentDb(-60)==12&&AmpNormalizationProcessor::adjustmentDb(-1)==-12,"Normalization compensation bounded at +/-12 dB");
    check(AmpNormalizationProcessor::adjustmentDb(-18)==0,"Normalization model at target -> unity");
    check(AmpNormalizationProcessor::adjustmentDb(NAN)==0&&AmpNormalizationProcessor::adjustmentDb(INFINITY)==0&&AmpNormalizationProcessor::adjustmentDb(1)==0&&AmpNormalizationProcessor::adjustmentDb(-101)==0,"Normalization invalid loudness -> unity without arbitrary fallback");
    const nlohmann::json valid={{"metadata",{{"loudness",-24.5}}}};
    check(namamp::modelLoudness(valid)==-24.5f,"NAM metadata reader preserves numeric loudness");
    bool invalid=true;
    for(const auto& json:std::vector<nlohmann::json>{{},{ {"metadata",nullptr} },{ {"metadata",{{"loudness","-24"}}} },{ {"metadata",{{"loudness",nullptr}}} },{ {"metadata",{{"loudness",1}}} }})invalid&=!namamp::modelLoudness(json).has_value();
    check(invalid,"Missing/null/string/invalid metadata is not invented or coerced");
    AudioBuffer<float> audio(2,128);tonegrid::AmpNormalizationProcessor dsp;dsp.prepare(48000);
    auto fill=[&]{for(int i=0;i<128;++i){audio.setSample(0,i,.25f);audio.setSample(1,i,-.4f);}};
    fill();dsp.setTarget(-60,false,true);dsp.process(audio);
    check(audio.getSample(0,0)==.25f&&audio.getSample(1,127)==-.4f,"Normalization disabled bypass is exact for distinct stereo inputs");
    dsp.setTarget(-60,true,false);fill();dsp.process(audio);
    check(audio.getSample(0,127)==.25f&&dsp.currentGain()==1,"Normalization never applied to a VST amp");
    dsp.setTarget(-24,true,true);fill();dsp.process(audio);
    const auto first=audio.getSample(0,0);check(first>.25f&&first<.251f&&dsp.currentGain()<Decibels::decibelsToGain(6.f),"Normalization enable starts a 25 ms smooth gain transition");
    for(int block=0;block<10;++block){fill();dsp.process(audio);}
    check(std::abs(audio.getSample(0,127)-.25f*Decibels::decibelsToGain(6.f))<1e-6&&std::abs(audio.getSample(1,127)+.4f*Decibels::decibelsToGain(6.f))<1e-6,"Settled normalization applies identical linear gain to both channels");
    dsp.setTarget(-24,true,false);fill();dsp.process(audio);
    check(audio.getSample(0,0)==.25f&&audio.getSample(1,127)==-.4f,"NAM-to-VST transition cannot leak previous NAM normalization gain into VST");
    dsp.setTarget(-24,true,true);for(int block=0;block<10;++block){fill();dsp.process(audio);}
    dsp.setTarget({},false,true);fill();dsp.process(audio);check(audio.getSample(0,0)>.49f&&audio.getSample(0,0)<.5f,"Normalization disable fades rather than jumping to unity");
    for(int block=0;block<10;++block){fill();dsp.process(audio);}
    fill();dsp.process(audio);check(audio.getSample(0,0)==.25f&&audio.getSample(1,127)==-.4f,"Normalization returns to exact unity after disable settles");
    const nlohmann::json enriched={{"metadata",{{"loudness",-19.796070772835293},{"Tonegrid_Normalization",{{"version",1},{"trim_db",14.56}}}}}};
    check(std::abs(namamp::modelNormalizationTrim(enriched)-14.56f)<1e-5,"ToneGrid v1 trim parsed as dB");
    check(std::abs(AmpNormalizationProcessor::adjustmentDb(namamp::modelLoudness(enriched),namamp::modelNormalizationTrim(enriched))-16.35607f)<1e-4,"Diezel trim adds to existing normalization without a combined 12 dB ceiling");
    auto malformed=enriched;bool ignored=true;
    for(const auto& field:std::vector<nlohmann::json>{nullptr,14.56,{{"version",2},{"trim_db",14.56}},{{"version",1.0},{"trim_db",14.56}},{{"version",true},{"trim_db",14.56}},{{"version",1},{"trim_db","14.56"}},{{"version",1},{"trim_db",25}},{{"version",1},{"trim_db",-25}},{{"version",1}},{{"trim_db",14.56}},{{"version",1},{"trim_db",NAN}},{{"version",1},{"trim_db",INFINITY}}})
    {malformed["metadata"]["Tonegrid_Normalization"]=field;ignored&=namamp::modelNormalizationTrim(malformed)==0.f;}
    check(ignored&&namamp::modelNormalizationTrim(valid)==0.f,"Missing/malformed/out-of-range/unknown-version trim ignored without rejecting NAM");
    check(AmpNormalizationProcessor::adjustmentDb({},4)==4&&AmpNormalizationProcessor::adjustmentDb(-24,-3)==3,"Valid trim works without loudness and supports attenuation");
    check(AmpNormalizationProcessor::adjustmentDb(-24,NAN)==6&&AmpNormalizationProcessor::adjustmentDb(-24,INFINITY)==6&&AmpNormalizationProcessor::adjustmentDb(-24,25)==6,"DSP defensively ignores non-finite/out-of-range trims");
    tonegrid::output::NormalizationNode node;MidiBuffer midi;node.prepareToPlay(48000,128);
    auto processNode=[&]{for(int b=0;b<12;++b){fill();node.processBlock(audio,midi);}};
    node.configure(true,true,-18,14.56f);processNode();
    check(std::abs(audio.getSample(0,127)-.25f*Decibels::decibelsToGain(14.56f))<1e-5,"Graph adapter applies measured trim to both output channels");
    tonegrid::output::Settings selected;selected.ampNormalizationEnabled=true;selected.ampNormalizationEnhanced=false;
    node.configure(selected.ampNormalizationEnabled,true,-24,tonegrid::output::selectedNormalizationTrim(selected,14.56f));processNode();
    check(std::abs(audio.getSample(0,127)-.25f*Decibels::decibelsToGain(6.f))<1e-6,"Partial selects NAM loudness only even when measured trim exists");
    selected.ampNormalizationEnhanced=true;
    node.configure(selected.ampNormalizationEnabled,true,-24,tonegrid::output::selectedNormalizationTrim(selected,4.f));processNode();
    check(std::abs(audio.getSample(0,127)-.25f*Decibels::decibelsToGain(10.f))<1e-5,"Full selects NAM loudness plus measured trim");
    check(namamp::optionalNormalizationTrim(nlohmann::json{{"metadata",{{"Tonegrid_Normalization",{{"version",1},{"trim_db",0}}}}}}).has_value()&&!namamp::optionalNormalizationTrim(valid),"A measured zero is distinguished from missing ToneGrid correction");
    node.configure(false,true,-18,14.56f);processNode();
    check(audio.getSample(0,127)==.25f&&audio.getSample(1,127)==-.4f,"OFF bypasses both standard normalization and trim after smoothing");
    node.configure(true,true,-18,14.56f);processNode();node.configure(true,false,-18,14.56f);fill();node.processBlock(audio,midi);
    check(audio.getSample(0,0)==.25f&&audio.getSample(1,127)==-.4f,"VST immediately bypasses both corrections");
    node.configure(true,true,-18,14.56f);processNode();node.configure(true,true,-18);processNode();
    check(audio.getSample(0,127)==.25f,"Switching to a capture without trim clears previous correction");
    const auto temp=File::getSpecialLocation(File::tempDirectory),root=temp.getChildFile("ToneGrid-normalization125-"+Uuid().toString());root.createDirectory();
    ScopeGuard cleanup{[&]{if(root.isAChildOf(temp))root.deleteRecursively();}};
    auto fixture=nlohmann::json{{"version","0.7.0"},{"architecture","Linear"},{"sample_rate",48000},{"config",{{"receptive_field",1},{"bias",false}}},{"weights",{1.0}},{"metadata",enriched["metadata"]}};
    const auto file=root.getChildFile("Measured.nam");file.replaceWithText(String(fixture.dump()));
    namamp::Library library(root);namamp::Entry entry;const auto read=library.read(file,entry);
    check(read.wasOk()&&std::abs(entry.normalizationTrimDb-14.56f)<1e-5,"Library retains trim during discovery");
    auto loaded=amp::loadNamAmp(library,{entry.identity,0,0});const auto* source=dynamic_cast<const amp::NamAmp*>(loaded.source.get());
    check(loaded.result.wasOk()&&source&&std::abs(source->normalizationTrimDb()-14.56f)<1e-5,"Actual NAM loader publishes trim to host source without changing model weights");
    report+=String(checks)+" amp normalization checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
