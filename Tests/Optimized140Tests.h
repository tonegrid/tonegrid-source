#pragma once
#include "Content/Amps/FactoryAmps.h"
#include "Content/Presets/ToneBank.h"
#include "Modules/Learn/UI/LearnView.h"
namespace optimized140Tests
{
inline bool run(const juce::File& resources,const juce::File& evidence,juce::String& report)
{
    using namespace juce;bool ok=true;int count=0;
    const auto check=[&](bool pass,const String& label){++count;ok&=pass;report+=String(pass?"PASS ":"FAIL ")+"optimized140 "+label+"\n";};
    const auto temp=File::getSpecialLocation(File::tempDirectory),root=temp.getChildFile("ToneGrid-optimized-"+Uuid().toString());
    ScopeGuard cleanup{[&]{if(root.isAChildOf(temp))root.deleteRecursively();}};
    const auto bundle=root.getChildFile("Bundle"),library=root.getChildFile("ToneGrid/NAM/Amps");bundle.createDirectory();library.createDirectory();
    const String fullName="03 - Crunch/Test/Full.nam",headName="03 - Crunch/Test - Head Only/Head.nam";
    const auto full=library.getChildFile(fullName),head=bundle.getChildFile(headName);full.getParentDirectory().createDirectory();head.getParentDirectory().createDirectory();
    full.replaceWithText(R"({"version":"0.7.0","architecture":"Linear","sample_rate":48000,"config":{"receptive_field":1,"bias":false},"weights":[1]})");
    head.replaceWithText(R"({"version":"0.7.0","architecture":"Linear","sample_rate":48000,"config":{"receptive_field":1,"bias":false},"weights":[0.5]})");
    namamp::Library reader(library);namamp::Entry old;check(reader.read(full,old).wasOk(),"original Full Rig identity is recorded before upgrade");
    const auto fallback=library.getChildFile("Factory/"+fullName);fallback.getParentDirectory().createDirectory();full.copyFileTo(fallback);
    const nlohmann::json archive={{"schemaVersion",1},{"captures",nlohmann::json::array({{{"file",fullName.toStdString()},{"sha256",old.identity.sha256.toStdString()},{"bytes",old.identity.bytes}}})}};
    const auto map=bundle.getChildFile("retired-full-rigs.json");map.replaceWithText(String(archive.dump()));
    nlohmann::json manifest={{"schemaVersion",2},{"ampCount",1},{"amps",nlohmann::json::array({{{"filename",headName.toStdString()},{"sha256",SHA256(head).toHexString().toStdString()}}})},{"assets",nlohmann::json::array({{{"filename","retired-full-rigs.json"},{"sha256",SHA256(map).toHexString().toStdString()}}})},{"archivedAmps",nlohmann::json::array({{{"filename",fullName.toStdString()},{"sha256",old.identity.sha256.toStdString()},{"bytes",old.identity.bytes}}})}};
    bundle.getChildFile("manifest.json").replaceWithText(String(manifest.dump()));std::atomic<bool> cancelled{false};
    check(tonegrid::factory::seed(bundle,library,cancelled).wasOk(),"upgrade installs the new Head and compatibility manifest");
    const auto privateRoot=namamp::factoryArchive::directory(library);
    check(!full.exists()&&!fallback.exists()&&privateRoot.getChildFile(fullName).existsAsFile()&&privateRoot.getChildFile("Factory/"+fullName).existsAsFile(),"both former factory locations archive outside the browsed library");
    namamp::Entry resolved;nlohmann::json payload;
    check(reader.resolve(old.identity,resolved,payload).wasOk()&&resolved.identity.sha256==old.identity.sha256&&payload["weights"][0]==1,"old user preset resolves the exact Full Rig network, never the Head replacement");
    auto alternate=old.identity;alternate.file="Factory/"+fullName;
    check(reader.resolve(alternate,resolved,payload).wasOk()&&resolved.identity.sha256==old.identity.sha256,"Factory-prefixed saved identities resolve the same original bytes");
    namamp::Catalog catalog;reader.refresh(catalog);check(catalog.entries.size()==1&&catalog.entries[0].identity.file==headName,"retired Full Rigs are absent from ordinary library discovery");
    auto undeclared=old.identity;undeclared.file="Other/Full.nam";
    check(reader.resolve(undeclared,resolved,payload).failed(),"an undeclared old path cannot load an unrelated archive file");
    full.getParentDirectory().createDirectory();full.replaceWithText("personal edit");
    check(reader.resolve(old.identity,resolved,payload).failed()&&full.loadFileAsString()=="personal edit","a modified same-name personal capture is not masked by archived fallback");
    auto changedManifest=manifest;changedManifest["contentVersion"]="second";bundle.getChildFile("manifest.json").replaceWithText(String(changedManifest.dump()));
    check(tonegrid::factory::seed(bundle,library,cancelled).wasOk()&&full.loadFileAsString()=="personal edit","future upgrades preserve a modified personal capture");
    full.deleteFile();privateRoot.getChildFile(fullName).appendText(" ");
    check(reader.resolve(old.identity,resolved,payload).failed(),"changed private archive bytes fail saved identity checks");
    changedManifest["archivedAmps"][0]["filename"]="../outside.nam";bundle.getChildFile("manifest.json").replaceWithText(String(changedManifest.dump()));
    check(tonegrid::factory::seed(bundle,library,cancelled).failed(),"archive path traversal is rejected before factory updates");
    // Upgrading a stock thumbnail must not hide a customized PNG with a new JPEG.
    const auto artBundle=root.getChildFile("ArtBundle"),artLibrary=root.getChildFile("ArtLibrary");
    artBundle.createDirectory();artLibrary.createDirectory();const auto aHead=artBundle.getChildFile(headName);aHead.getParentDirectory().createDirectory();head.copyFileTo(aHead);
    const auto newArt=artBundle.getChildFile("03 - Crunch/Test - Head Only/thumbnail.jpg");newArt.replaceWithText("stock jpeg bytes");
    const auto custom=artLibrary.getChildFile("03 - Crunch/Test - Head Only/thumbnail.png");custom.getParentDirectory().createDirectory();custom.replaceWithText("personal png bytes");
    const auto meta=custom.getSiblingFile(".tonegrid-tone3000.json");meta.replaceWithText(R"({"image":"thumbnail.png"})");
    auto artManifest=manifest;artManifest.erase("archivedAmps");artManifest["assets"]=nlohmann::json::array({{{"filename","03 - Crunch/Test - Head Only/thumbnail.jpg"},{"sha256",SHA256(newArt).toHexString().toStdString()}}});
    artManifest["retiredAssets"]=nlohmann::json::array({{{"filename","03 - Crunch/Test - Head Only/thumbnail.png"},{"sha256",std::string(64,'a')}}});
    artBundle.getChildFile("manifest.json").replaceWithText(String(artManifest.dump()));
    check(tonegrid::factory::seed(artBundle,artLibrary,cancelled).wasOk()&&custom.loadFileAsString()=="personal png bytes"&&!custom.withFileExtension("jpg").exists()&&meta.loadFileAsString().contains("thumbnail.png"),"thumbnail optimization preserves a personal PNG and its metadata precedence");
    const auto factory=resources.getChildFile("FactoryAmps"),ir=resources.getChildFile("FactoryCabinetIR");
    const auto inventory=nlohmann::json::parse(factory.getChildFile("manifest.json").loadFileAsString().toStdString());
    check(inventory.at("ampCount")==1358&&inventory.at("archivedAmps").size()==146,"optimized inventory contains 1358 NAMs and 146 declared retired Full Rigs");
    bool models=true,art=true;for(const auto& a:inventory.at("amps")){const auto f=factory.getChildFile(String(a.at("filename").get<std::string>()));models&=f.existsAsFile()&&SHA256(f).toHexString()==String(a.at("sha256").get<std::string>());}
    for(const auto& a:inventory.at("assets")){const auto f=factory.getChildFile(String(a.at("filename").get<std::string>()));art&=f.existsAsFile()&&SHA256(f).toHexString()==String(a.at("sha256").get<std::string>());}
    check(models&&art,"all retained factory NAM and asset identities validate");
    bool retired=true;for(const auto& a:inventory.at("archivedAmps"))retired&=!factory.getChildFile(String(a.at("filename").get<std::string>())).exists();check(retired,"retired Full Rigs are physically absent from the app payload");
    const auto cab=nlohmann::json::parse(ir.getChildFile("catalog.json").loadFileAsString().toStdString());
    bool irs=true;for(const auto& a:cab.at("irs")){const auto f=ir.getChildFile(String(a.at("file").get<std::string>()));irs&=f.existsAsFile()&&SHA256(f).toHexString()==String(a.at("sha256").get<std::string>());}
    check(irs&&cab.at("irs").size()==2233,"all 2233 cabinet IR waveforms match their pinned identities, including 18 normalized ToneGrid additions");
    tonegrid::tonebank::Bank bank;check(tonegrid::tonebank::parse(resources.getChildFile("ToneBank/tonebank.json").loadFileAsString(),bank).wasOk()&&bank.presets.size()==101,"the complete 101-preset ToneBank parses with explicit IR dependencies");
    int headPresets=0;bool presetIRs=true;
    for(const auto& p:bank.presets)if(p.state.cabinet){++headPresets;const auto f=ir.getChildFile(p.state.cabinet->file);presetIRs&=f.existsAsFile()&&SHA256(f).toHexString()==p.state.cabinet->sha256;}
    check(headPresets==33&&presetIRs,"all 33 converted ToneBank presets reference an exact installed IR");
    auto broken=nlohmann::json::parse(resources.getChildFile("ToneBank/tonebank.json").loadFileAsString().toStdString());
    for(auto& p:broken["presets"])if(p["state"].contains("cabinetIR")){p["state"]["cabinetIR"]["sha256"]=std::string(64,'0');break;}
    tonegrid::tonebank::Bank unchanged=bank;check(tonegrid::tonebank::parse(String(broken.dump()),unchanged).failed()&&unchanged.presets.size()==101,"inconsistent required IR identity rejects the bank transactionally");
    tonegrid::libraryui::AmpCaptureInfo typeInfo(factory);const auto types=nlohmann::json::parse(factory.getChildFile("capture-info.json").loadFileAsString().toStdString());bool headsCorrect=true;int typeChecks=0;
    for(const auto& a:inventory.at("amps"))if(String(a.at("filename").get<std::string>()).contains("Head Only/")&&(String(a.at("filename").get<std::string>()).contains("Fortin")||String(a.at("filename").get<std::string>()).contains("Mark VII")||String(a.at("filename").get<std::string>()).contains("Diezel")))
    {tonegrid::libraryui::Capture c;c.gear="wrong";typeInfo.apply(c,String(a.at("sha256").get<std::string>()));headsCorrect&=c.gear=="amp";++typeChecks;}
    check(headsCorrect&&typeChecks==24,"mixed Fortin/Mark VII/VH4 packs classify all 24 complete Heads correctly");
    evidence.createDirectory();tonegrid::learn::View learn;learn.configure(resources.getChildFile("Learn/lessons.json"),root.getChildFile("favourites.json"),root.getChildFile("web"),true);
    for(int width:{900,1660})
    {learn.setSize(width,820);const auto image=learn.content().createComponentSnapshot(learn.content().getLocalBounds());MemoryOutputStream png;const auto pixel=image.getPixelAt(28,30);check(pixel.getBrightness()>.95f&&PNGImageFormat().writeImageToStream(image,png)&&evidence.getChildFile("learn-logo-"+String(width)+".png").replaceWithData(png.getData(),png.getDataSize()),"official logo white tile renders at width "+String(width));}
    report+=String(count)+" optimized 1.1.40 checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
