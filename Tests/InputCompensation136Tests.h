#pragma once
#include "NamAmpTests.h"
#include "CalibrationWarnings.h"

namespace inputCompensation136Tests
{
inline bool run(const juce::File& factory,juce::String& report)
{
    using namespace juce;
    bool ok=true;int checks=0;
    auto check=[&](bool value,const String& text){++checks;ok&=value;report+=String(value?"PASS ":"FAIL ")+text+"\n";};
    nlohmann::json json={{"metadata",{{"Tonegrid_Input_Compensation",{{"version",1},{"trim_db",12.0}}}}}};
    check(namamp::inputCompensation(json)==12.f,"versioned listening compensation reads in dB, without inventing dBu references");
    const auto info=tonegrid::calibration::metadata(json);
    check(!info.input&&!info.output&&info.status()==tonegrid::calibration::CaptureStatus::uncalibrated,"listening adjustment never claims electrical calibration");
    for(const auto& invalid:nlohmann::json::array({nullptr,true,"12",25,-25}))
    {auto bad=json;bad["metadata"]["Tonegrid_Input_Compensation"]["trim_db"]=invalid;check(!namamp::inputCompensation(bad),"malformed or out-of-range compensation is neutral");}
    auto bad=json;bad["metadata"]["Tonegrid_Input_Compensation"]["version"]=2;
    check(!namamp::inputCompensation(bad)&&!namamp::inputCompensation(nlohmann::json::object()),"unknown version and absent optional adjustment are neutral");
    namTests::Trace trace;namamp::State controls{};
    amp::NamAmpProcessor cubic(std::make_unique<namTests::Cubic>(trace),controls,false,12.f);
    cubic.prepareToPlay(48000,128);
    const auto render=[&](){AudioBuffer<float> b(2,128);MidiBuffer midi;for(int i=0;i<128;++i)b.setSample(0,i,.02f);cubic.processBlock(b,midi);return b.getSample(0,127);};
    const auto expected=std::pow(.02f*Decibels::decibelsToGain(12.f),3);
    check(std::abs(render()-expected)<1e-8f,"+12 dB drives nonlinear inference before the model at displayed Gain zero");
    tonegrid::calibration::ReferenceRouting routing;routing.chunks=1;routing.known[0]=false;cubic.setUpstreamReference(&routing);
    cubic.setCalibration({false,12.5f});
    check(std::abs(render()-expected)<1e-8f,"unknown upstream reference and disabled calibration retain the listening adjustment");
    cubic.prepareToPlay(48000,128);cubic.controls(0,0);
    check(std::abs(render()-expected)<1e-8f&&trace.resets==1,"reprepare does not duplicate compensation or reset the model");
    cubic.controls(-12,0);for(int i=0;i<12;++i)render();
    check(std::abs(render()-std::pow(.02f,3))<1e-9f,"user Gain remains additive and can cancel the built-in adjustment");
    const auto document=nlohmann::json::parse(factory.getChildFile("manifest.json").loadFileAsString().toStdString());
    namamp::Library library(factory);int compensated=0;
    for(const auto& item:document.at("amps"))
    {
        const auto relative=String(item.at("filename").get<std::string>());
        if(!relative.contains("/Fender Blues Junior Tweed/"))continue;
        ++compensated;namamp::Entry entry;nlohmann::json payload;
        const auto result=library.read(factory.getChildFile(relative),entry,&payload);
        check(result.wasOk()&&entry.inputCompensationDb==12&&entry.hasNormalizationTrim,"factory capture exposes input compensation and separate measured output trim");
        if(result.failed())continue;
        check(!entry.calibration.input&&!entry.calibration.output&&!entry.calibration.invalid,"factory capture remains explicitly uncalibrated");
        auto loaded=amp::loadNamAmp(library,{entry.identity,0,0});auto* native=dynamic_cast<amp::NamAmp*>(loaded.source.get());
        check(native&&native->inputCompensationDb()==12&&native->captureDescription().contains("not electrical calibration"),"production loader applies metadata and tooltip explains the listening adjustment");
        if(!native)continue;
        AudioProcessorGraph graph;auto node=native->addToGraph(graph);
        auto* actual=dynamic_cast<amp::NamAmpProcessor*>(node->getProcessor());actual->setCalibration({true,12.5f});actual->prepareToPlay(48000,128);
        payload["metadata"].erase("Tonegrid_Input_Compensation");payload["metadata"].erase("Tonegrid_Normalization");
        nam::DspLoadOptions options;options.prewarm=false;auto raw=nam::get_dsp(payload,options);
        raw->SetPrewarmOnReset(false);raw->Reset(48000,128);tonegrid::namquality::apply(*raw,false);raw->prewarm();
        AudioBuffer<float> buffer(2,128);MidiBuffer midi;std::array<float,128> input{},output{};bool equal=true;
        for(int block=0;block<16;++block)
        {
            for(int i=0;i<128;++i){const auto sample=.02f*std::sin(float(block*128+i)*.071f);buffer.setSample(0,i,sample);input[size_t(i)]=sample*Decibels::decibelsToGain(12.f);}
            float* in=input.data();float* out=output.data();raw->process(&in,&out,128);actual->processBlock(buffer,midi);
            for(int i=0;i<128;++i)equal&=std::isfinite(buffer.getSample(0,i))&&std::abs(buffer.getSample(0,i)-output[size_t(i)])<1e-6f;
        }
        check(equal,"new factory capture at Gain zero matches original NAM driven by +12 dB");
        native->unload();graph.clear();
    }
    check(compensated==20,"all 20 publisher-authorized Blues Junior captures are covered");
    report+=String(checks)+" input compensation checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";
    return ok;
}
}
