#pragma once
#include "Reference/ToneBank132.h"
#include "Content/Presets/PresetShare.h"
#include "MissingCapturesView.h"

namespace toneBank133Tests
{
inline bool run(const tonegrid::tonebank::Json& shipped,const board::Processor& processor,juce::String& report)
{
    using namespace juce;using namespace tonegrid;using namespace tonegrid::tonebank;
    bool ok=true;int checks=0;const auto check=[&](bool pass,const String& name){++checks;ok&=pass;report+=(pass?"PASS ":"FAIL ")+String("ToneBank133 ")+name+"\n";};
    const auto parseBank=[](const Json& json,Bank& bank){return parse(String(json.dump()),bank);};
    auto original=shipped;original["presets"].erase(original["presets"].begin()+100,original["presets"].end());original["presetCount"]=100; // Fixed boundary fixture, independent of growing shipped bank.
    auto larger=original;auto extra=larger["presets"][0];extra["id"]="101-growth-regression";extra["rank"]=101;larger["presets"].push_back(extra);larger["presetCount"]=101;
    Bank bank;check(parseBank(larger,bank).wasOk()&&bank.presets.size()==101&&bank.presets.back().rank==101,"101 presets accepted with stable rank ordering");
    PresetBrowser browser;browser.setBank(bank);browser.showToneBank(true);check(browser.visibleCount()==101,"browser displays every entry above 100");size_t clicked=0;browser.onTone=[&](size_t i){clicked=i;};browser.activateRow(100);check(clicked==100,"entry 101 can be selected");
    auto single=original;single["presets"].erase(single["presets"].begin()+1,single["presets"].end());single["presetCount"]=1;check(parseBank(single,bank).wasOk()&&bank.presets.size()==1,"smaller non-empty banks accepted");
    auto empty=single;empty["presets"]=Json::array();empty["presetCount"]=0;check(parseBank(empty,bank).failed()&&bank.presets.size()==1,"empty bank rejected atomically");
    auto badCount=larger;badCount["presetCount"]=100;check(parseBank(badCount,bank).failed(),"declared count must match actual entries");
    auto duplicate=larger;duplicate["presets"][100]["rank"]=1;check(parseBank(duplicate,bank).failed(),"rank uniqueness enforced beyond 100");
    auto edited=single;auto& state=edited["presets"][0]["state"];state["pedals"][0]["parameters"]["mixPercent"]=35;
    state["amp"]["eq"]={{"enabled",true},{"bassDb",1},{"midDb",-2},{"trebleDb",3},{"lowCut",true},{"lowCutHz",100},{"highCut",true},{"highCutHz",9500},
        {"lowMidDb",-3},{"lowMidHz",400},{"lowMidQ",.75},{"presenceDb",4},{"presenceHz",4200},{"presenceQ",1.5},{"depthDb",-2.5},{"depthHz",85}};
    check(parseBank(edited,bank).wasOk(),"all extended controls parse");
    const auto& parsed=bank.presets.front();const auto& eq=parsed.state.outputDsp->ampEq;
    check(eq.enabled&&eq.lowCut&&eq.highCut&&eq.lowCutHz==100&&eq.highCutHz==9500&&eq.lowMidDb==-3&&eq.lowMidHz==400&&eq.lowMidQ==.75f
        &&eq.presenceDb==4&&eq.presenceHz==4200&&eq.presenceQ==1.5f&&eq.depthDb==-2.5f&&eq.depthHz==85,"presence, depth, both cuts and parametric values retained");
    check(std::abs(parsed.state.pedals[0].mix-.35f)<.0001f,"compressor wet/dry percent converted to engine mix");
    auto snapshot=parsed.state;snapshot.sourceType=amp::SourceType::Nam;snapshot.namAmp.model={parsed.amp.file,parsed.amp.sha256,100};
    const auto identity=[&](int id,int preset){return processor.identity(id,preset);};const auto xml=board::encode(snapshot,{},identity);board::State restored;
    check(board::decode(xml,{},identity,restored).wasOk()&&output::encode(*restored.outputDsp).isEquivalentTo(xml.getChildByName("OutputDSP"),true)
        &&std::abs(restored.pedals[0].mix-.35f)<.0001f,"JSON to saved User Preset XML and back preserves extended controls");
    const auto shared=sharing::body(restored,identity,"Synthetic test references");check(shared.contains(board::encode(restored,{},identity).toString())&&shared.contains("presenceDb=\"4")&&shared.contains("depthDb=\"-2.5"),"Share includes the same complete musical values without sending email");
    auto legacy=single;auto& legacyState=legacy["presets"][0]["state"];legacyState["amp"].erase("levelNormalizationMode");legacyState["amp"]["eq"]={{"bassDb",0},{"midDb",0},{"trebleDb",0},{"enabled",false}};
    legacyState["pedals"][0]["presetId"]=1;legacyState["pedals"][0]["parameters"].erase("mixPercent");
    check(parseBank(legacy,bank).wasOk()&&std::abs(bank.presets[0].state.pedals[0].mix-.7f)<.0001f&&bank.presets[0].state.outputDsp->ampEq.presenceDb==0
        &&bank.presets[0].state.outputDsp->ampEq.depthDb==0&&!bank.presets[0].state.outputDsp->ampEq.lowCut,"older JSON with omitted optional fields retains previous defaults");
    for(const auto& field:std::initializer_list<std::pair<const char*,Json>>{{"presenceDb",13},{"depthDb",7},{"lowCutHz",0},{"highCutHz",500},{"presenceQ",0},{"lowCut",1},{"highCut",nullptr}})
    {auto bad=edited;bad["presets"][0]["state"]["amp"]["eq"][field.first]=field.second;check(parseBank(bad,bank).failed(),"reject invalid "+String(field.first));}
    auto badMix=edited;badMix["presets"][0]["state"]["pedals"][0]["parameters"]["mixPercent"]=101;check(parseBank(badMix,bank).failed(),"reject out-of-range compressor mix");
    auto partial=single;partial["presets"][0]["state"]["amp"]["levelNormalizationMode"]="partial";check(parseBank(partial,bank).wasOk()&&!bank.presets[0].state.outputDsp->ampNormalizationEnhanced,"explicit Partial normalization retained");
    partial["presets"][0]["state"]["amp"]["levelNormalizationMode"]="off";check(parseBank(partial,bank).failed(),"inconsistent normalization mode rejected");
    tonebank132Reference::Bank older;
    check(tonebank132Reference::parse(String(original.dump()),older).failed(),"actual 1.1.32 parser rejects the new library revision");
    auto oldCompatible=original;oldCompatible["libraryRevision"]="tonegrid-library-1.1.25-private-beta-r1";oldCompatible["targetToneGridVersion"]="1.1.25";
    // Recreate the historical syntax: cabinet dependencies were introduced later.
    // Current Head + IR recall is exercised separately by Optimized140Tests.
    for(auto& preset:oldCompatible["presets"])
    {
        // This is a synthetic historical-format fixture. Current publisher
        // presets may legitimately exceed the old amp gain/volume limits.
        preset["state"]["amp"]["gainDb"]=0;
        preset["state"]["amp"]["volumeDb"]=0;
        preset["state"].erase("cabinetIR");
        auto& resources=preset["requiredResources"];
        for(auto it=resources.begin();it!=resources.end();)
            if(it->value("kind",std::string{})=="cabinet")it=resources.erase(it);else ++it;
    }
    check(tonebank132Reference::parse(String(oldCompatible.dump()),older).wasOk(),"old parser accepts 100 entries with its original library identity");
    check(parseBank(oldCompatible,bank).wasOk()&&bank.presets.size()==100,"1.1.33 also reads the 1.1.25–1.1.32 library identifier");
    check(!File(TONEGRID_BANK_FILE).getSiblingFile("transition-legacy.json").existsAsFile(),"only the modern ToneBank is shipped; old-format parsing remains covered by synthetic fixtures");
    auto ignored=oldCompatible;ignored["presets"][0]["state"]["amp"]["eq"]=state["amp"]["eq"];ignored["presets"][0]["state"]["pedals"][0]["parameters"]["mixPercent"]=35;
    check(tonebank132Reference::parse(String(ignored.dump()),older).wasOk()&&older.presets[0].state.outputDsp->ampEq.presenceDb==0
        &&older.presets[0].state.outputDsp->ampEq.depthDb==0&&older.presets[0].state.pedals[0].mix!=.35f,"old parser ignores extra fields rather than reproducing their sound");
    auto oldLarge=larger;oldLarge["libraryRevision"]="tonegrid-library-1.1.25-private-beta-r1";check(tonebank132Reference::parse(String(oldLarge.dump()),older).failed(),"old parser still rejects banks larger than 100");
    namamp::Catalog compatibilityCatalog;compatibilityCatalog.entries.push_back({{"03 - Crunch/Orange OR60/Current.nam","98f2af328c7625835c109c796d26151fd74ecad5edafb53c72e4a76a1c90e704",100}});
    const Resource legacyOrange{"03 - Crunch/Orange OR60/Previous.nam","e5631b910bfeedeb48b9fae0602db45440b177c2ecd2e26f1c5bcca34f9096f7","Sweet Delivery"};
    const auto compatible=resolveAmp(legacyOrange,compatibilityCatalog);check(compatible&&compatible->sha256==compatibilityCatalog.entries[0].identity.sha256,"known metadata-only OR60 revision resolves through its explicit verified identity alias");
    for(const auto* hash:{"82fa446758f20d7994dec3df6578d31a1c4aafc0743727a2684480db646cde85","964672fd9b2ab0bb7aed8a00a69fdfa855e0885623ded9486629b34558fdfdb0","7aa1084d47124551a24e3596db848fe21355e86829d75c8673ef190412f1b22b","dfbdf7f0fedffe6cd7a6486cacc17bef1e1ab5244b8217660726969ad7b0285c","6dd5d6e9a91a5a623878e7d8d47828dbc5332c19e7dc2e7b4628b931a700b6b9"})
        check(compatibleAmpHash(hash)=="3bb5c6ecfcf59a8c600db95df7da7c7b1e2ae447f7ee30904cffbcda85702707","withdrawn Silver Jubilee cached ToneBank identity maps to Wizard");
    auto unknown=legacyOrange;unknown.sha256=String::repeatedString("f",64);check(!resolveAmp(unknown,compatibilityCatalog),"identity alias never matches unrelated amp hashes");
    MissingCapturesView warning("Synthetic preset",{"Missing amp: Test Amp.nam","Missing pedal: Test OD.nam"});
    auto* details=dynamic_cast<TextEditor*>(warning.findChildWithID("missing-captures-details"));
    check(details&&details->getText().contains("Test Amp.nam")&&details->getText().contains("Test OD.nam")&&details->getText().contains("Your current sound has been kept")
        &&warning.findChildWithID("missing-captures-update-status")&&warning.popupFooterActions().size()==3,
        "scrollable warning lists every capture, preserves the sound and includes update availability with fixed recovery actions");
    report+=String(checks)+" ToneBank133 compatibility and control checks\n";return ok;
}
}
