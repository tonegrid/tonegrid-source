#pragma once
#include "OnlineUpdates.h"
#include "Content/Amps/FactoryAmps.h"

namespace distribution116Tests
{
class FakeTransport final:public tonegrid::t3k::Transport
{
public:
    juce::String version="1.1.36",bank;int versionStatus=200,bankStatus=200,calls=0;
    juce::StringArray addresses;
    tonegrid::t3k::Response perform(const tonegrid::t3k::Request& request,const std::atomic<bool>&) override
    {
        ++calls;addresses.add(request.url);const bool app=request.url==tonegrid::updates::versionUrl;
        const auto& raw=app?version:bank;
        return {app?versionStatus:bankStatus,juce::MemoryBlock(raw.toRawUTF8(),raw.getNumBytesAsUTF8()),{}};
    }
};
inline bool run(const juce::File& factory,juce::String& report)
{
    using namespace juce;using namespace tonegrid;using namespace updates;
    bool ok=true;int count=0;const auto check=[&](bool pass,const String& label){++count;ok=ok&&pass;report+=(pass?"PASS ":"FAIL ")+String("Distribution116 ")+label+"\n";};
    check(Version::parse("1.1.24")>Version::parse("1.1.9"),"numeric version comparison");
    check(Version::parse("2.0.0")>Version::parse("1.99.999"),"major version comparison");
    check(Version::parse(String::charToString(0xfeff)+"1.1.24\n")==Version::parse("1.1.24"),"BOM and whitespace accepted");
    for(const auto& invalid:StringArray{"<html>1.1.25</html>","1.1","1..16","1.1.24-beta","-1.1.24","9999999.0.0","1.1.24\n1.1.25"})check(!Version::parse(invalid),"reject malformed version "+invalid.replaceCharacter('\n',' '));
    const int64 now=1800000000000LL;
    check(checkInterval==72LL*60*60*1000&&!due(now-71LL*60*60*1000,now)&&due(now-72LL*60*60*1000,now),"both automatic checks use exactly 72 elapsed hours");
    check(due(0,now)&&!due(now,now)&&!due(now-checkInterval+1,now)&&due(now-checkInterval,now)&&due(now+1,now),"72-hour boundary and future clock recovery");
    const auto root=File::getSpecialLocation(File::tempDirectory).getChildFile("ToneGrid-116-tests-"+Uuid().toString());root.createDirectory();
    const auto preference=root.getChildFile("Online/checks.xml"),cache=tonebank::cacheForApplicationData(root);
    Schedule dates{now,now};check(dates.write(preference).wasOk(),"persist check dates");
    check(Schedule::read(preference).application==now&&Schedule::read(preference).bank==now,"dates survive service restart");
    {Service service(preference,cache,"1.1.24");check(!service.start(true,true,false,now+checkInterval-1)&&!service.busy(),"launch within 72 hours starts no network worker");}
    FakeTransport io;std::atomic<bool> cancelled{false};
    const auto shipped=File(TONEGRID_BANK_FILE);io.bank=shipped.loadFileAsString();
    auto seeded=tonebank::readCache(cache,shipped);check(seeded.result.wasOk(),"offline bundled bank installed");const auto firstHash=SHA256(cache).toHexString();
    auto result=fetch(io,cancelled,preference,cache,"1.1.36",{},true,true,now);
    check(result.applicationStatus.wasOk()&&!result.newVersion&&!result.bankChanged&&io.calls==2,"current version and unchanged bank");
    check(io.addresses[0]=="https://www.tonegrid.net/lastversion.txt"&&io.addresses[1]=="https://www.tonegrid.net/tonebank-modern.json","application version and separate modern ToneBank use fixed HTTPS endpoints");
    io.version="1.1.37";result=fetch(io,cancelled,preference,cache,"1.1.36",{},true,false,now);
    check(result.newVersion&&result.latestVersion=="1.1.37","new update detected");
    io.version="1.1.19";result=fetch(io,cancelled,preference,cache,"1.1.36",{},true,false,now);check(!result.newVersion&&result.applicationStatus.wasOk(),"older server version causes no downgrade");
    io.version="1.1.37";io.versionStatus=404;result=fetch(io,cancelled,preference,cache,"1.1.36",{},true,false,now);check(result.applicationStatus.failed()&&!result.newVersion,"HTTP error never reported as an update");
    io.versionStatus=200;io.version="<html>error</html>";result=fetch(io,cancelled,preference,cache,"1.1.36",{},true,false,now);check(result.applicationStatus.failed(),"invalid text rejected");
    io.bank="invalid JSON";result=fetch(io,cancelled,preference,cache,"1.1.36",{},false,true,now);check(result.bankStatus.failed()&&SHA256(cache).toHexString()==firstHash,"invalid bank preserves cache");
    io.bankStatus=503;io.bank=shipped.loadFileAsString();result=fetch(io,cancelled,preference,cache,"1.1.36",{},false,true,now);check(result.bankStatus.failed()&&SHA256(cache).toHexString()==firstHash,"offline server preserves cache");
    io.bankStatus=200;auto json=tonebank::Json::parse(io.bank.toStdString());json["targetToneGridVersion"]="1.1.37";io.bank=String(json.dump());result=fetch(io,cancelled,preference,cache,"1.1.36",{},false,true,now);check(result.bankStatus.failed()&&SHA256(cache).toHexString()==firstHash,"future bank cannot break older app");
    json["targetToneGridVersion"]="1.1.36";json["presets"][0]["state"]["amp"]["gainDb"]=-3;io.bank=String(json.dump());result=fetch(io,cancelled,preference,cache,"1.1.36",{},false,true,now);check(result.bankStatus.wasOk()&&result.bankChanged&&result.bank.presets[0].state.namAmp.gainDb==-3,"valid compatible refresh installed");
    const auto remoteHash=SHA256(cache).toHexString();seeded=tonebank::readCache(cache,shipped);check(seeded.result.wasOk()&&SHA256(cache).toHexString()==remoteHash,"remote refresh survives offline restart");
    auto legacyBank=json;legacyBank["presets"].erase(legacyBank["presets"].begin()+100,legacyBank["presets"].end());legacyBank["presetCount"]=100;legacyBank["libraryRevision"]="tonegrid-library-1.1.25-private-beta-r1";legacyBank["targetToneGridVersion"]="1.1.25";legacyBank["presets"][0]["state"]["amp"]["gainDb"]=-2;
    legacyBank["presets"][0]["state"]["pedals"][0]["parameters"].erase("mixPercent");legacyBank["presets"][0]["state"]["amp"]["eq"]={{"bassDb",0},{"midDb",0},{"trebleDb",0},{"enabled",false}};io.bank=String(legacyBank.dump());
    result=fetch(io,cancelled,preference,cache,"1.1.36",{},false,true,now);check(result.bankStatus.wasOk()&&result.bankChanged&&result.bank.presets.size()==100,"1.1.36 downloads and installs a compatible legacy 100-preset bank");
    const auto legacyHash=SHA256(cache).toHexString();seeded=tonebank::readCache(cache,shipped);check(seeded.result.wasOk()&&SHA256(cache).toHexString()==legacyHash&&seeded.bank.libraryRevision=="tonegrid-library-1.1.25-private-beta-r1","downloaded legacy bank survives an offline restart");
    const auto changedBundle=root.getChildFile("new-bundled.json");json["presets"][0]["state"]["amp"]["gainDb"]=-4;changedBundle.replaceWithText(String(json.dump()));seeded=tonebank::readCache(cache,changedBundle);check(seeded.result.wasOk()&&seeded.bank.presets[0].state.namAmp.gainDb==-4,"updated package seeds corrected bank once");
    changedBundle.replaceWithText("invalid");seeded=tonebank::readCache(cache,changedBundle);check(seeded.result.wasOk()&&seeded.bank.presets[0].state.namAmp.gainDb==-4,"damaged bundle retains valid cache");
    cancelled=true;const int before=io.calls;result=fetch(io,cancelled,preference,cache,"1.1.36",{},true,true,now);check(io.calls==before,"cancelled work performs no HTTP");cancelled=false;
    tonebank::Bank bank;check(tonebank::parse(shipped.loadFileAsString(),bank).wasOk(),"corrected bank validates");
    check(std::all_of(bank.presets.begin(),bank.presets.end(),[](const auto& p){return p.state.outputDsp&&p.state.outputDsp->ampNormalizationEnhanced&&(p.state.outputDsp->ampNormalizationEnabled||(p.artist=="John Mayer"&&p.song=="Slow Dancing in a Burning Room")||(p.artist=="Jimi Hendrix"&&p.song=="Little Wing"));}),"Full normalization retained except publisher-specified Slow Dancing and Little Wing OFF settings");
    check(bank.libraryRevision==board::libraryRevision,"bank targets exact installed capture library");
    {
        const auto previousBundle=root.getChildFile("PreviousFactory135");check(factory.copyDirectoryTo(previousBundle),"stage previous 1.1.36 factory library");
        auto previous=tonebank::Json::parse(previousBundle.getChildFile("manifest.json").loadFileAsString().toStdString());
        auto& entries=previous["amps"];std::set<std::string> added;
        for(const auto& entry:entries)if(entry.value("origin",std::string{})=="publisher-authorized-rxguitar-addition-2026-10-17"||entry.value("origin",std::string{})=="publisher-authorized-gloo-addition-2026-10-17")added.insert(entry.at("filename").get<std::string>());
        entries.erase(std::remove_if(entries.begin(),entries.end(),[&](const auto& e){return added.contains(e.at("filename").template get<std::string>());}),entries.end());previous["ampCount"]=entries.size();
        auto& changes=previous["metadataUpdates"];changes.erase(std::remove_if(changes.begin(),changes.end(),[&](const auto& e){return added.contains(e.at("filename").template get<std::string>());}),changes.end());
        previousBundle.getChildFile("manifest.json").replaceWithText(String(previous.dump()));
        const auto existingProfile=root.getChildFile("ExistingProfile/NAM/Amps");auto upgradeResult=factory::seed(previousBundle,existingProfile,cancelled);
        check(added.size()==96&&upgradeResult.wasOk()&&existingProfile.findChildFiles(File::findFiles,true,"*.nam").size()==366,"existing 366-amp profile installs correctly before the content update");
        const auto personal=existingProfile.getChildFile("Personal/My own capture.nam");personal.getParentDirectory().createDirectory();personal.replaceWithText("personal capture kept");
        const auto presets=root.getChildFile("ExistingProfile/Rigs-v1/slot-001.tonexrig");presets.getParentDirectory().createDirectory();presets.replaceWithText("existing user preset kept");
        upgradeResult=factory::seed(factory,existingProfile,cancelled);bool allAdded=upgradeResult.wasOk();
        for(const auto& name:added)allAdded&=existingProfile.getChildFile(String(name)).existsAsFile();
        check(allAdded&&existingProfile.findChildFiles(File::findFiles,true,"*.nam").size()==463,"changed manifest installs all 96 additions into existing AppData without a reset");
        check(personal.loadFileAsString()=="personal capture kept"&&presets.loadFileAsString()=="existing user preset kept","content update preserves personal captures and user presets");
        const auto marker=existingProfile.getParentDirectory().getChildFile("factory-amps-sha256.txt");
        check(marker.loadFileAsString()==SHA256(factory.getChildFile("manifest.json")).toHexString()&&factory::seed(factory,existingProfile,cancelled).wasOk(),"updated factory marker makes subsequent startup idempotent");
        auto original136=tonebank::Json::parse(factory.getChildFile("manifest.json").loadFileAsString().toStdString());auto& originalEntries=original136["amps"];
        originalEntries.erase(std::remove_if(originalEntries.begin(),originalEntries.end(),[](const auto& e){return e.value("origin",std::string{})=="publisher-authorized-gloo-addition-2026-10-17";}),originalEntries.end());
        original136["ampCount"]=originalEntries.size();previousBundle.getChildFile("manifest.json").replaceWithText(String(original136.dump()));
        const auto previous136Profile=root.getChildFile("Existing136Profile/NAM/Amps");
        check(factory::seed(previousBundle,previous136Profile,cancelled).wasOk()&&previous136Profile.findChildFiles(File::findFiles,true,"*.nam").size()==442,"original 1.1.36 profile contains the 442 previously published factory amps");
        check(factory::seed(factory,previous136Profile,cancelled).wasOk()&&previous136Profile.findChildFiles(File::findFiles,true,"*.nam").size()==462,"gloo content revision adds all 20 models to an existing 1.1.36 profile");
    }
    const auto library=root.getChildFile("NAM/Amps");auto installed=factory::seed(factory,library,cancelled);
    check(installed.wasOk()&&library.findChildFiles(File::findFiles,true,"*.nam").size()==462,"462 factory amps installed without network");
    const auto files=library.findChildFiles(File::findFiles,true,"*.nam");if(files.isEmpty())return false;const auto one=files[0];const auto userHash=SHA256(one).toHexString();installed=factory::seed(factory,library,cancelled);check(installed.wasOk()&&SHA256(one).toHexString()==userHash,"factory seeding idempotent");
    const auto upgrade=root.getChildFile("UpgradeFactory");check(factory.copyDirectoryTo(upgrade),"stage isolated upgrade fixture");
    auto upgradeJson=tonebank::Json::parse(upgrade.getChildFile("manifest.json").loadFileAsString().toStdString());
    StringArray retiredFiles;for(auto& entry:upgradeJson["retiredAmps"])
        if(String(entry.at("filename").get<std::string>()).contains("Silver_Jubilee"))
        {
            entry["sha256"]=userHash.toStdString();const String name(entry.at("filename").get<std::string>());
            const auto old=library.getChildFile(name);old.getParentDirectory().createDirectory();one.copyFileTo(old);retiredFiles.add(name);
        }
    check(retiredFiles.size()==5,"all five retired Silver Jubilee names covered");
    const auto custom=library.getChildFile("Factory/"+retiredFiles[0]);custom.getParentDirectory().createDirectory();custom.replaceWithText("modified personal model");
    const auto artSource=upgrade.getChildFile(String(upgradeJson["assets"][0]["filename"].get<std::string>()));
    const String retiredArt(upgradeJson["retiredAssets"][0]["filename"].get<std::string>());
    upgradeJson["retiredAssets"][0]["sha256"]=SHA256(artSource).toHexString().toStdString();const auto oldArt=library.getChildFile(retiredArt);artSource.copyFileTo(oldArt);
    upgrade.getChildFile("manifest.json").replaceWithText(String(upgradeJson.dump()));installed=factory::seed(upgrade,library,cancelled);
    bool removed=installed.wasOk()&&!oldArt.exists();for(const auto& name:retiredFiles)removed&=!library.getChildFile(name).exists();
    check(removed,"upgrade removes all unchanged withdrawn copies and their old thumbnail");
    check(custom.loadFileAsString()=="modified personal model","upgrade preserves modified personal files");
    upgradeJson["retiredAssets"][0]["filename"]="../thumbnail.jpg";upgrade.getChildFile("manifest.json").replaceWithText(String(upgradeJson.dump()));
    check(factory::seed(upgrade,library,cancelled).failed()&&custom.loadFileAsString()=="modified personal model","unsafe retired artwork path rejected without changing user files");
    one.replaceWithText("user-edited model");installed=factory::seed(factory,library,cancelled);check(installed.wasOk()&&one.loadFileAsString()=="user-edited model","user edits survive subsequent startup");
    check(root.isAChildOf(File::getSpecialLocation(File::tempDirectory))&&root.deleteRecursively(),"temporary fixtures removed");
    report+=String(count)+" distribution 1.1.24 checks\n";return ok;
}
}
