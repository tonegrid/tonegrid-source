#pragma once
#include "Platform/LegacyProfileCleanup.h"
#include "Core/PresetLoadQueue.h"
#include "MissingCapturesView.h"
#include "PresetBrowser.h"
#include "CabinetProcessor.h"
namespace factoryRefinements142Tests
{
inline bool run(const juce::File& resources,juce::String& report)
{
    using namespace juce;using namespace tonegrid;bool ok=true;int checks=0;
    const auto check=[&](bool yes,const String& text){++checks;ok&=yes;report+=String(yes?"PASS ":"FAIL ")+text+"\n";};
    core::PresetLoadQueue queue;for(int i=0;i<2000;++i)queue.request(i%7,1000.);
    check(!queue.take(1149.),"Rapid selections wait for the last click to settle");
    auto chosen=queue.take(1150.);check(chosen&&chosen->slot==1999%7&&!queue.take(2000.),"Two thousand clicks produce one pending final choice");
    const auto old=chosen->revision;for(int i=0;i<1000;++i)queue.request(i%5,2000.);
    check(!queue.current(old),"A preparation cannot publish after a newer choice");
    chosen=queue.take(2150.);queue.cancel();check(chosen&&!queue.current(chosen->revision)&&!queue.take(4000.),"Cancellation invalidates in-flight work and clears pending choices");
    cabinet::State state{"4x12/Example/IR.wav",String::repeatedString("a",64),true,true},decoded;
    auto xml=cabinet::encode(state);check(cabinet::decode(xml,decoded)&&decoded.bypassed&&decoded.locked&&decoded.sha256==state.sha256,"Cabinet power state round-trips with exact IR and lock identity");
    xml.removeAttribute("bypassed");check(cabinet::decode(xml,decoded)&&!decoded.bypassed,"Existing presets without cabinet power metadata keep IR enabled");
    xml.setAttribute("bypassed","invalid");check(!cabinet::decode(xml,decoded),"Malformed power metadata is rejected without publishing a partial state");
    cabinet::Node node;node.prepareToPlay(48000,128);node.enable(false);AudioBuffer<float> audio(2,128);MidiBuffer midi;
    for(int c=0;c<2;++c)for(int i=0;i<128;++i)audio.setSample(c,i,float(i-64)/128);
    const AudioBuffer<float> original(audio);node.processBlock(audio,midi);bool unchanged=true;
    for(int c=0;c<2;++c)for(int i=0;i<128;++i)unchanged&=audio.getSample(c,i)==original.getSample(c,i);
    check(unchanged,"Cabinet OFF passes the amp signal through exactly, including without a loaded IR");
    cabinet::Panel panel;panel.setSize(300,140);panel.update(nullptr,"External cabinet IR",true,true,true,false);
    check(panel.power.isEnabled()&&!panel.power.getToggleState()&&panel.lock.getToggleState(),"Head Only exposes cabinet power without changing the IR lock");
    panel.update(nullptr,"Cabinet included",false,true,false,false);check(!panel.power.isEnabled()&&!panel.browse.isEnabled(),"Full Rig retains the disabled Cabinet area");
    MissingCapturesView missing("Example",{"Missing amp"});auto* label=dynamic_cast<Label*>(missing.findChildWithID("missing-captures-update-status"));auto* update=dynamic_cast<Button*>(missing.findChildWithID("missing-captures-update"));
    missing.setUpdateResult(false,{},false);check(label&&label->getText().endsWith("No")&&update&&!update->isEnabled(),"No update is stated explicitly and cannot trigger a download");
    missing.setUpdateResult(true,"1.1.43",false);check(label->getText().contains("Yes (1.1.43)")&&update->isEnabled(),"Available updates show their version and enable Update ToneGrid");
    missing.setUpdateResult(false,{},true);check(label->getText().contains("Unable to check")&&!update->isEnabled(),"Network failure is not misreported as no available update");
    const auto temp=File::getSpecialLocation(File::tempDirectory).getChildFile("ToneGrid-cleanup143-"+Uuid().toString());temp.createDirectory();
    struct Cleanup{File file;~Cleanup(){file.deleteRecursively();}} cleanup{temp};
    const auto legacy=temp.getChildFile("ToneGridIR"),current=paths::userFolder(temp);legacy.createDirectory();current.createDirectory();
    legacy.getChildFile("personal.txt").replaceWithText("Current user data");current.getChildFile("previous.txt").replaceWithText("Earlier profile");
    paths::cleanupLegacyProfile(temp);
    check(!legacy.exists()&&current.getChildFile("previous.txt").loadFileAsString()=="Earlier profile"&&!current.getChildFile("personal.txt").exists(),"Legacy profile is removed without transferring or changing the canonical profile");
    paths::cleanupLegacyProfile(temp);
    check(!temp.getChildFile("ToneGrid-before-1.1.42").exists()&&current.getChildFile("previous.txt").loadFileAsString()=="Earlier profile","No migration backup is created; repeated optional cleanup is harmless");
    const auto raw=resources.getChildFile("ToneBank/tonebank.json").loadFileAsString();tonebank::Bank bank;
    const auto parsed=tonebank::parse(raw,bank);check(parsed.wasOk(),"Complete expanded ToneBank parses: "+parsed.getErrorMessage());if(parsed.failed())return false;
    int nuno=0,uncreditedPolice=0,owner=0;for(const auto& p:bank.presets)
    {
        if(p.creator=="Nuno Riobom"&&p.version=="1.02")++nuno;
        if(p.creator.isEmpty()&&p.artist=="The Police"&&p.song=="Every Breath You Take")++uncreditedPolice;
        if(((p.artist=="Ozzy Osbourne"&&p.song=="Crazy Train")||(p.artist=="Prince"&&p.song=="Purple Rain"))&&p.version=="1.01"&&p.creator.isEmpty())++owner;
    }
    check(nuno==12&&uncreditedPolice==1&&owner==2,"Updated publisher revisions and anonymous owner presets display their approved credits");
    auto data=nlohmann::json::parse(raw.toStdString());data["presets"][0]["creator"]="Nuno";data["presets"][0]["version"]="1.01";
    tonebank::Bank extended;check(tonebank::parse(String(data.dump()),extended).wasOk()&&extended.presets[0].creator=="Nuno"&&extended.presets[0].version=="1.01","Creator and revision display metadata do not change audio schema compatibility");
    data["presets"][0]["creator"]="ToneGrid";data["presets"][0].erase("version");data["presets"][0].erase("revision");data["presets"][0].erase("metadata");check(tonebank::parse(String(data.dump()),extended).wasOk()&&extended.presets[0].creator.isEmpty()&&extended.presets[0].version.isEmpty(),"ToneGrid and absent revisions display empty cells");
    bool thumbs=true;int folders=0;for(const auto& folder:{resources.getChildFile("FactoryAmps"),resources.getChildFile("FactoryCabinetIR")})
    {std::set<String> packs;for(const auto& file:folder.findChildFiles(File::findFiles,true,"*"))if(file.hasFileExtension("nam;wav"))packs.insert(file.getParentDirectory().getFullPathName());for(const auto& name:packs){++folders;const File pack(name);bool found=false;for(const auto* ext:{"jpg","png","jpeg"}){const auto file=pack.getChildFile(String("thumbnail.")+ext);if(file.existsAsFile())found|=ImageFileFormat::loadFrom(file).isValid();}thumbs&=found;}check(!folder.getChildFile("Artwork").exists(),"No central Artwork folder remains in "+folder.getFileName());}
    check(thumbs&&folders>150,"Every amp and cabinet pack has a decodable local thumbnail");
    const auto manifest=nlohmann::json::parse(resources.getChildFile("StarterPresets/manifest.json").loadFileAsString().toStdString());
    const auto models=XmlDocument::parse(resources.getChildFile("Models/catalog.xml"));board::Processor algorithms;
    const auto identity=[&](int p,int n){if(!board::isNam(p))return algorithms.identity(p,n);if(auto* group=models->getChildByAttribute("id",String(p)))if(auto* entry=group->getChildElement(n))return entry->getStringAttribute("sha256");return String{};};
    bool starters=manifest.at("presets").size()==7&&resources.getChildFile("StarterPresets").findChildFiles(File::findFiles,false,"*.tonexrig").size()==7;
    for(const auto& record:manifest.at("presets")){const auto file=resources.getChildFile("StarterPresets").getChildFile(String(record.at("file").get<std::string>()));const auto doc=XmlDocument::parse(file);board::State preset;PluginDescription none;starters&=doc&&board::decode(*doc,none,identity,preset).wasOk()&&preset.name==String(record.at("name").get<std::string>())&&SHA256(file).toHexString()==String(record.at("sha256").get<std::string>());}
    check(starters,"Exactly seven first-launch User Presets decode with their declared names and hashes");
    report+=String(checks)+" factory refinements checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
