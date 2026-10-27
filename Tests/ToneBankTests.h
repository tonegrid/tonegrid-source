#pragma once
#include "Content/Presets/ToneBank.h"
#include "PresetBrowser.h"
#include "Content/Presets/RigCodec.h"
#include "ToneBank133Tests.h"
namespace toneBankTests
{
inline bool run(const juce::File& models,juce::String& report)
{
    using namespace juce;using namespace tonegrid;using namespace tonegrid::tonebank;
    bool ok=true;int checks=0;const auto check=[&](bool pass,const String& name){++checks;ok=ok&&pass;report+=(pass?"PASS ":"FAIL ")+String("ToneBank ")+name+"\n";};
    const File shipped(TONEGRID_BANK_FILE);const auto original=shipped.loadFileAsString();Bank bank;
    check(parse(original,bank).wasOk()&&bank.presets.size()==101,"supplied bank parses with exactly 101 entries");if(bank.presets.empty())return false;
    check(bank.genres.size()==24,"24 genres derived from entries");
    auto json=Json::parse(original.toStdString());Bank unchanged=bank;
    const auto invalid=[&](Json value,const String& reason){auto result=parse(String(value.dump()),unchanged);check(result.failed()&&unchanged.id==bank.id&&unchanged.presets.size()==bank.presets.size()&&unchanged.presets[0].id==bank.presets[0].id,"reject "+reason+" without publishing partial bank");};
    auto changed=json;changed["schemaVersion"]=3;invalid(changed,"unsupported schema");
    changed=json;changed["bankId"]="";invalid(changed,"empty bank ID");
    changed=json;changed["presetCount"]=99;invalid(changed,"wrong count");
    changed=json;changed["presets"][1]["id"]=changed["presets"][0]["id"];invalid(changed,"duplicate ID");
    changed=json;changed["presets"][1]["rank"]=1;invalid(changed,"duplicate rank");
    changed=json;changed["presets"][0]["state"]["amp"]["source"]="VST3";invalid(changed,"VST3 source");
    changed=json;changed["presets"][0]["state"]["input"]["gainDb"]=99;invalid(changed,"gain outside range");
    changed=json;changed["presets"][0]["state"]["input"]["gateEnabled"]=1;invalid(changed,"numeric boolean");
    changed=json;changed["presets"][0]["state"]["pitch"]["mode"]="UNKNOWN";invalid(changed,"pitch enum");
    changed=json;changed["presets"][0]["state"]["pitch"]["semitones"]=.5;invalid(changed,"fractional semitone");
    changed=json;changed["presets"][0]["state"]["mod"]["type"]="Rotary";invalid(changed,"MOD enum");
    changed=json;changed["presets"][0]["state"]["reverb"]["type"]="Unknown";invalid(changed,"Reverb enum");
    changed=json;changed["presets"][0]["state"]["pedals"][0]["id"]=13;invalid(changed,"pedal ID");
    changed=json;changed["presets"][0]["state"]["pedals"][0]["name"]="Wrong";invalid(changed,"pedal name mismatch");
    changed=json;changed["presets"][0]["state"]["pedals"][1]["resource"]["relativePath"]="../secret.nam";invalid(changed,"path traversal");
    changed=json;changed["presets"][0]["state"]["amp"]["resource"]["sha256"]="invalid";invalid(changed,"invalid SHA syntax");
    changed=json;changed["presets"][0]["requiredResources"][0]["sha256"]=std::string(64,'a');invalid(changed,"inconsistent required identity");
    changed=json;std::reverse(changed["presets"].begin(),changed["presets"].end());Bank reordered;check(parse(String(changed.dump()),reordered).wasOk()&&reordered.presets.front().rank==1&&reordered.presets.back().rank==bank.presets.back().rank,"deterministic rank sort independent of JSON ordering");
    for(const auto& p:bank.presets)
    {
        bool valid=p.state.sourceType==amp::SourceType::Nam&&output::valid(*p.state.outputDsp)&&namamp::validControls(p.state.namAmp.gainDb,p.state.namAmp.volumeDb);
        for(int id=0;id<board::count;++id){auto controls=p.state.pedals[(size_t)id];valid=valid&&board::valid(controls,id);}
        check(valid,"all capture IDs and control ranges valid at rank "+String(p.rank));
    }
    check(filter(bank,"All","").size()==bank.presets.size(),"All gives every entry");
    check(!filter(bank,"all","mEtAlLiCa").empty(),"case insensitive artist search");
    check(filter(bank,"All","Master of Puppets").size()==1,"song search");
    check(!filter(bank,"All","MKIIC Bull Sheet").empty(),"amp metadata search");
    check(!filter(bank,"All","BLEND #1.nam").empty(),"amp filename search");
    check(!filter(bank,"mEtAl","Metallica").empty()&&filter(bank,"Blues","Metallica").empty(),"combined AND genre/search");
    check(filter(bank,"All","NoSuchArtist987").empty(),"empty search result");
    PresetBrowser browser;browser.setBank(bank);browser.setUsers({"001 â€” test","002 â€” empty"},{true,false});
    check(!browser.toneTabActive()&&browser.visibleCount()==2,"USER PRESETS default tab");
    auto* userList=dynamic_cast<ListBox*>(browser.findChildWithID("preset-list"));
    auto* savedRow=userList?userList->getComponentForRowNumber(0):nullptr;auto* emptyRow=userList?userList->getComponentForRowNumber(1):nullptr;
    auto* rename=savedRow?dynamic_cast<Button*>(savedRow->findChildWithID("preset-rename")):nullptr;auto* erase=savedRow?dynamic_cast<Button*>(savedRow->findChildWithID("preset-delete")):nullptr;
    auto* emptyRename=emptyRow?dynamic_cast<Button*>(emptyRow->findChildWithID("preset-rename")):nullptr;auto* emptyErase=emptyRow?dynamic_cast<Button*>(emptyRow->findChildWithID("preset-delete")):nullptr;
    check(rename&&erase&&rename->isEnabled()&&erase->isEnabled()&&emptyRename&&emptyErase&&!emptyRename->isEnabled()&&!emptyErase->isEnabled(),"Saved User Presets have pencil/trash buttons; empty slots cannot rename/delete");
    int recalled=-1,renamed=-1,deleted=-1;browser.onUser=[&](int index){recalled=index;};browser.onRename=[&](int index){renamed=index;};browser.onDelete=[&](int index){deleted=index;};
    if(rename)rename->onClick();if(erase)erase->onClick();check(renamed==0&&deleted==0&&recalled==-1,"Preset actions target the correct slot without recalling or loading it");
    browser.showToneBank(true);check(browser.toneTabActive()&&browser.visibleCount()==(int)bank.presets.size(),"TONEBANK tab");
    check(userList&&!userList->getComponentForRowNumber(0),"Read-only ToneBank rows never receive User Preset actions");
    auto* reload=browser.findChildWithID("tonebank-reload");check(reload&&reload->isVisible()&&reload->isEnabled(),"Reload visible and enabled");
    const auto reloadTip=dynamic_cast<TextButton*>(reload)->getTooltip();
    check(reloadTip.contains("JSON")&&reloadTip.containsIgnoreCase("official"),"Reload tooltip explains personal JSON and official catalogue reloading");
    browser.setSearch("Metallica");browser.setGenre("Blues");check(browser.visibleCount()==0,"UI combined filter empty state");browser.setGenre("All");check(browser.visibleCount()>0,"UI immediately updates results");
    size_t selected=999;browser.onTone=[&](size_t index){selected=index;};browser.activateRow(0);check(selected<bank.presets.size()&&bank.presets[selected].artist.containsIgnoreCase("Metallica"),"filtered row retains stable preset identity");
    const auto temporary=File::getSpecialLocation(File::tempDirectory).getChildFile("ToneGrid-ToneBank-tests-"+Uuid().toString());
    const auto cache=cacheForApplicationData(temporary);auto cached=readCache(cache,shipped);check(cached.result.wasOk()&&cache.existsAsFile()&&cache.loadFileAsString()==original,"first-use seed preserves supplied JSON content");
    const auto cacheHash=SHA256(cache).toHexString();cached=readCache(cache,temporary.getChildFile("missing-shipped.json"));check(cached.result.wasOk()&&SHA256(cache).toHexString()==cacheHash,"valid local cache works offline without package access and remains untouched");
    check(install(cache,"bad JSON").failed()&&SHA256(cache).toHexString()==cacheHash,"invalid future installation leaves previous cache intact");
    cache.replaceWithText("bad JSON");cached=readCache(cache,shipped);check(cached.result.wasOk()&&cached.bank.presets.size()==101&&cached.warning.contains("Invalid local")&&cache.loadFileAsString()==original,"invalid cache falls back and seeds validated bundled bank");
    cache.replaceWithText("bad JSON");cached=readCache(cache,temporary.getChildFile("missing.json"));check(cached.result.failed()&&cached.bank.presets.empty(),"no valid bank reports error without partial data");
    namamp::Catalog catalog;catalog.entries.push_back({{"moved/subfolder/different-name.nam",bank.presets[0].amp.sha256,100},"","",{}});
    auto resolved=resolveAmp(bank.presets[0].amp,catalog);check(resolved&&resolved->file=="moved/subfolder/different-name.nam","recursive hash identity survives folder and filename changes");
    catalog.entries[0].identity.sha256=String::repeatedString("a",64);catalog.entries[0].identity.file=bank.presets[0].amp.file;check(!resolveAmp(bank.presets[0].amp,catalog),"same filename with different hash never substitutes amp");
    board::Processor processor;check(processor.load(models).wasOk(),"current NAM pedal catalog validated");
    ok=toneBank133Tests::run(json,processor,report)&&ok;
    Preset missing=bank.presets[0];auto& pedal=missing.state.pedals[1];pedal.enabled=true;missing.captures[1].sha256=String::repeatedString("a",64);missing.state.pedals[2].enabled=true;missing.captures[2].sha256=String::repeatedString("b",64);
    auto plan=prepare(missing,{},processor,models);check(!plan.replaceAmp&&plan.warnings.size()==3&&!plan.state.pedals[1].enabled&&!plan.state.pedals[2].enabled&&plan.state.pedals[12].enabled==missing.state.pedals[12].enabled,"missing amp and two enabled captures aggregate warnings and bypass only affected pedals");
    check(plan.warnings[0]==missingAmp(missing.amp)&&plan.warnings[1].containsIgnoreCase("808 OD")&&plan.warnings[2].containsIgnoreCase("Horse OD"),"warnings identify each exact missing resource");
    missing.state.pedals[1].enabled=false;missing.state.pedals[2].enabled=false;plan=prepare(missing,{},processor,models);check(plan.warnings.size()==1,"missing dormant pedal resources do not warn");
    check(shipped.loadFileAsString()==original,"browsing/filtering/loading plan never changes supplied bank");
    check(temporary.isAChildOf(File::getSpecialLocation(File::tempDirectory))&&temporary.deleteRecursively(),"temporary fixtures removed");
    report+=String(checks)+" ToneBank parser/cache/filter checks\n";return ok;
}
}


