#pragma once
#include "CalibrationWarnings.h"
#include "Content/Amps/FactoryAmps.h"
#include "Content/Amps/AmpCaptureInfo.h"
#include "NamAmp.h"

namespace release134Tests
{
inline bool run(const juce::File& payload,juce::String& report)
{
    using namespace juce;using namespace tonegrid;
    bool ok=true;int checks=0;const auto check=[&](bool pass,const String& label){++checks;ok&=pass;report+=(pass?"PASS ":"FAIL ")+String("Release134 ")+label+"\n";};
    const auto root=File::getSpecialLocation(File::tempDirectory).getChildFile("ToneGrid-release134-"+Uuid().toString());root.createDirectory();
    struct Cleanup{File root;~Cleanup(){if(root.isAChildOf(File::getSpecialLocation(File::tempDirectory)))root.deleteRecursively();}} cleanup{root};
    nlohmann::json model={{"version","0.7.0"},{"architecture","Linear"},{"sample_rate",48000},{"config",{{"receptive_field",1},{"bias",false}}},{"weights",{.75}},
        {"metadata",{{"gear_type","amp_cab"},{"input_level_dbu",6.5},{"loudness",-26.757983107032274}}}};
    const auto original=model;const auto inputOnly=calibration::metadata(model);
    calibration::WarningStore warnings(root.getChildFile("warnings.xml"));const auto hash=SHA256("example",7).toHexString();
    check(!warnings.shouldWarn(inputOnly,hash,{true,true}),"valid input-only final AMP+CAB with normalization ON needs no popup");
    check(warnings.shouldWarn(inputOnly,hash,{true,false}),"normalization OFF retains the partial-calibration warning");
    check(warnings.shouldWarn(inputOnly,hash,{false,true}),"upstream model still warns when missing output affects the next model");
    check(inputOnly.status()==calibration::CaptureStatus::partial&&inputOnly.description().contains("6.50 dBu")&&inputOnly.description().contains("Output: Not available"),"metadata details remain honest and accessible");
    auto ac30=model;ac30["metadata"].erase("input_level_dbu");check(warnings.shouldWarn(calibration::metadata(ac30),hash,{true,true}),"AC30-style capture missing both references still warns");
    for(const auto& value:std::initializer_list<nlohmann::json>{nullptr,"6.5",true,1000})
    {
        auto bad=model;bad["metadata"]["input_level_dbu"]=value;check(warnings.shouldWarn(calibration::metadata(bad),hash,{true,true}),"malformed input is never exempt");
        bad=model;bad["metadata"]["output_level_dbu"]=value;check(warnings.shouldWarn(calibration::metadata(bad),hash,{true,true}),"present but malformed output is never treated as absent");
    }
    auto unknown=model;unknown["metadata"].erase("gear_type");check(warnings.shouldWarn(calibration::metadata(unknown),hash,{true,true}),"unknown gear type receives no blanket final-stage waiver");
    auto invalid=model;invalid["metadata"]="malformed";check(warnings.shouldWarn(calibration::metadata(invalid),hash,{true,true}),"malformed metadata retains warning");
    check(original==model,"warning decisions do not mutate capture data");
    check(warnings.shouldWarn(inputOnly,hash,{true,false}),"suppressed normalized context does not acknowledge the raw context");
    const auto source=root.getChildFile("Factory"),library=root.getChildFile("User/NAM/Amps");source.createDirectory();library.createDirectory();
    const String filename="03 - Crunch/Example/Model.nam";const auto installed=library.getChildFile(filename),bundled=source.getChildFile(filename);
    installed.getParentDirectory().createDirectory();bundled.getParentDirectory().createDirectory();installed.replaceWithText(String(model.dump()));
    const auto oldHash=SHA256(installed).toHexString();auto restored=model;restored["metadata"]["Tonegrid_Normalization"]={{"version",1},{"trim_db",2.91}};
    bundled.replaceWithText(String(restored.dump()));const auto newHash=SHA256(bundled).toHexString();
    nlohmann::json inventory={{"schemaVersion",2},{"ampCount",1},{"amps",nlohmann::json::array({{{"filename",filename.toStdString()},{"sha256",newHash.toStdString()}}})},
        {"metadataUpdates",nlohmann::json::array({{{"filename",filename.toStdString()},{"previousSha256",oldHash.toStdString()},{"sha256",newHash.toStdString()}}})}};
    source.getChildFile("manifest.json").replaceWithText(String(inventory.dump()));std::atomic<bool> cancelled{false};
    check(factory::seed(source,library,cancelled).wasOk()&&SHA256(installed).toHexString()==newHash&&!library.getChildFile("Factory/"+filename).exists(),"upgrade restores only known metadata in place without a duplicate capture");
    check(factory::seed(source,library,cancelled).wasOk()&&SHA256(installed).toHexString()==newHash,"metadata upgrade is idempotent");
    const auto personal=root.getChildFile("Personal/NAM/Amps"),custom=personal.getChildFile(filename);custom.getParentDirectory().createDirectory();custom.replaceWithText("personal changes");
    check(factory::seed(source,personal,cancelled).wasOk()&&custom.loadFileAsString()=="personal changes"&&personal.getChildFile("Factory/"+filename).existsAsFile(),"personal modifications survive and receive a separate bundled capture");
    const auto unsafe=root.getChildFile("Unsafe/NAM/Amps"),unchanged=unsafe.getChildFile(filename);unchanged.getParentDirectory().createDirectory();unchanged.replaceWithText(String(model.dump()));
    auto corrupt=restored;corrupt["weights"][0]=.5;bundled.replaceWithText(String(corrupt.dump()));inventory["amps"][0]["sha256"]=SHA256(bundled).toHexString().toStdString();inventory["metadataUpdates"][0]["sha256"]=inventory["amps"][0]["sha256"];
    source.getChildFile("manifest.json").replaceWithText(String(inventory.dump()));check(factory::seed(source,unsafe,cancelled).failed()&&SHA256(unchanged).toHexString()==oldHash,"metadata-only migration refuses changed model weights before touching the old capture");
    nam::DspLoadOptions options;options.prewarm=false;auto beforeDsp=nam::get_dsp(model,options),afterDsp=nam::get_dsp(restored,options);
    beforeDsp->SetPrewarmOnReset(false);afterDsp->SetPrewarmOnReset(false);beforeDsp->Reset(48000,128);afterDsp->Reset(48000,128);
    std::array<float,128> input{},beforeAudio{},afterAudio{};for(int i=0;i<128;++i)input[size_t(i)]=.1f*std::sin(float(i)*.13f);float* in=input.data();float* a=beforeAudio.data();float* b=afterAudio.data();beforeDsp->process(&in,&a,128);afterDsp->process(&in,&b,128);
    check(beforeAudio==afterAudio,"restoring optional normalization metadata leaves raw NAM inference bit-exact");
    for(const auto& update:namamp::normalizationMetadataUpdates)
    {
        namamp::State state{{update.filename,update.oldHash,100},-3.f,2.f};auto xml=namamp::encode(state);namamp::State decoded;
        check(namamp::decode(xml,decoded)&&decoded.model.sha256==update.sha256&&decoded.model.bytes==update.bytes&&decoded.gainDb==-3&&decoded.volumeDb==2,"saved metadata-only identities migrate without resetting Gain or Volume");
    }
    const auto factoryFolder=payload.getChildFile("FactoryAmps");const auto doc=nlohmann::json::parse(factoryFolder.getChildFile("manifest.json").loadFileAsString().toStdString());
    libraryui::AmpCaptureInfo info(factoryFolder);int described=0,measured=0;bool normalization=true;
    for(const auto& entry:doc.at("amps"))
    {
        libraryui::Capture c;info.apply(c,String(entry.at("sha256").get<std::string>()));libraryui::sidecar(c,factoryFolder.getChildFile(String(entry.at("filename").get<std::string>())),String(entry.at("sha256").get<std::string>()));described+=c.description.isNotEmpty()&&c.author.isNotEmpty()&&libraryui::authorSearch(c).isNotEmpty();
        const auto json=nlohmann::json::parse(factoryFolder.getChildFile(String(entry.at("filename").get<std::string>())).loadFileAsString().toStdString());if(json["metadata"].contains("Tonegrid_Normalization")){++measured;normalization&=namamp::optionalNormalizationTrim(json).has_value();}
    }
    check(described==462,"all 462 current amp identities resolve descriptions and credited creator links");
    check(measured==262&&normalization,"262 measured normalization fields remain valid after restoring the three OR60 values");
    report+=String(checks)+" Release134 checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
