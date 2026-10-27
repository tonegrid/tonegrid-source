#pragma once
#include "Content/Calibration/InterfaceProfiles.h"
#include "Content/Presets/RigStore.h"
#include "Shared/UI/Library/LibraryNavigation.h"
#include "Modules/Jam/UI/JamView.h"
#include "Modules/Learn/UI/LearnView.h"
#include "GuitarCalibrationWizard.h"
#include "MidiBindings.h"
namespace audit142Tests {
inline bool run(const juce::File& resources, juce::String& report) {
    using namespace juce;using namespace tonegrid;
    bool ok=true;
    const auto check=[&](bool value,const String& label){ok&=value;report+=String(value?"PASS ":"FAIL ")+label+"\n";};
    const auto root=File::getSpecialLocation(File::tempDirectory).getChildFile("ToneGrid-audit142-"+Uuid().toString());
    root.createDirectory();
    struct Cleanup {File folder;~Cleanup(){folder.deleteRecursively();}} cleanup{root};
    const auto defaults=String::fromUTF8(ToneGridApplicationAssets::interfaceprofiles_json,ToneGridApplicationAssets::interfaceprofiles_jsonSize);
    auto catalogue=nlohmann::json::parse(defaults.toStdString());
    std::vector<calibration::InterfaceProfile> profiles;
    check(calibration::decodeInterfaceProfiles(defaults,profiles).wasOk()&&profiles.size()==catalogue["profiles"].size(),"All shipped interface profiles pass the new runtime validator");
    const auto oldCount=profiles.size();
    auto extra=catalogue["profiles"][0];extra["id"]="audit142-extra-interface";extra["aliases"]=nlohmann::json::array();extra["model"]="Extra interface";extra["futureMetadata"]={{"notes","kept in source"}};
    catalogue["profiles"].push_back(extra);
    check(calibration::decodeInterfaceProfiles(String(catalogue.dump()),profiles).wasOk()&&profiles.size()==oldCount+1,"An additional interface and optional future metadata load without a compiled count");
    const auto personal=root.getChildFile("Content/Calibration/interface-profiles.json");personal.getParentDirectory().createDirectory();personal.replaceWithText(String(catalogue.dump()));
    check(calibration::interfaceProfiles(personal).size()==oldCount+1,"Calibration uses a writable external JSON override");
    calibration::GuitarWizard wizard({},personal);
    auto* menu=dynamic_cast<ComboBox*>(wizard.findChildWithID("calibration-profile"));
    check(menu&&menu->getNumItems()==int(oldCount)+2,"The actual calibration dropdown grows with the external catalogue");
    auto broken=catalogue;broken["profiles"].push_back(extra);
    check(calibration::decodeInterfaceProfiles(String(broken.dump()),profiles).failed()&&profiles.size()==oldCount+1,"Duplicate profiles fail atomically without replacing a valid catalogue");
    broken=catalogue;broken["profiles"][0]["instrumentChannels"]={64};
    check(calibration::decodeInterfaceProfiles(String(broken.dump()),profiles).failed(),"Out-of-range interface channels are rejected");
    personal.replaceWithText("{ invalid");String warning;
    check(calibration::interfaceProfiles(personal,&warning).size()==oldCount&&warning.isNotEmpty(),"Malformed interface JSON keeps bundled defaults and explains the error");
    const auto copy=root.getChildFile("Content/Jam/tracks.json");
    check(content::createTemplate(copy,resources.getChildFile("Jam/backing-tracks.json")).wasOk(),"Explicit content editing creates a missing template");
    copy.replaceWithText("personal edit");
    check(content::createTemplate(copy,resources.getChildFile("Jam/backing-tracks.json")).wasOk()&&copy.loadFileAsString()=="personal edit","Opening content settings cannot overwrite personal edits");

    using namespace libraryui;
    const std::set<String> folders{"1x12","1x12/Clean Pack","4x12","4x12/Lead Pack","Empty"};
    const std::vector<NavigationItem> items{{"1x12/Clean Pack/a.wav","Clean A","Fender clean SM57",true,0},{"4x12/Lead Pack/b.wav","Lead B","Marshall lead SM57",false,1}};
    auto rows=visibleRows(folders,items,BrowseMode::all,{},{});
    check(rows.size()==2&&rows[0].pack&&rows[1].pack,"Both browser adapters share physical pack navigation");
    rows=visibleRows(folders,items,BrowseMode::folder,"1x12",{});
    check(rows.size()==1&&rows[0].path=="1x12/Clean Pack"&&rows[0].count==1,"Folder navigation shows actual child packs and counts");
    rows=visibleRows(folders,items,BrowseMode::folder,"1x12/Clean Pack",{});
    check(rows.size()==1&&!rows[0].pack&&rows[0].entry==0,"Opening a pack exposes its physical capture entry");
    rows=visibleRows(folders,items,BrowseMode::favorites,{},{});
    check(rows.size()==1&&rows[0].path.endsWith("a.wav"),"Shared favorites navigation selects capture identities supplied by the adapter");
    rows=visibleRows(folders,items,BrowseMode::all,{},"Marshall SM57");
    check(rows.size()==1&&rows[0].entry==1,"Shared search requires all terms and returns the correct capture");

    auto xml=XmlDocument::parse(resources.getChildFile("StarterPresets/slot-001.tonexrig"));
    check(xml!=nullptr,"Use a real shipped preset as the compatibility fixture");
    if(xml){
        const board::RigIdentityLookup identity=[&](int id,int){for(auto* node:xml->getChildIterator())if(node->hasTagName("Pedal")&&node->getIntAttribute("id")==id)return node->getStringAttribute("model");return String{};};
        xml->setAttribute("futureAuthor","A beginner's notes");
        xml->createNewChildElement("Metadata")->createNewChildElement("FutureTags")->addTextElement("lead, bright & personal");
        board::State state;
        check(board::decode(*xml,{},identity,state).wasOk(),"Additive descriptive fields remain loadable in the supported preset schema");
        auto saved=board::encode(state,{},identity);
        check(saved.getStringAttribute("futureAuthor")=="A beginner's notes"&&saved.getChildByName("Metadata")&&saved.getChildByName("Metadata")->getAllSubText()=="lead, bright & personal","Unknown descriptive metadata survives load and save");
        check(state.presetId.isNotEmpty()&&saved.getStringAttribute("presetId")==state.presetId,"Presets have stable identities independent of their display position");
        auto legacy=state;legacy.presetId.clear();
        check(board::encode(legacy,{},identity).toString()==board::encode(legacy,{},identity).toString(),"Pure preset encoding remains deterministic when upgrading a legacy identity");
        const auto store=board::RigStore(root.getChildFile("Presets"),root.getChildFile("Legacy"));
        check(store.capacity()==rig::slotCount,"An empty library keeps its familiar initial slot layout");
        check(store.save(128,state,{},identity).result.wasOk()&&store.contains(128)&&store.capacity()==130,"Preset 129 saves and grows the library beyond the MIDI bank");
        check(store.save(500,state,{},identity).result.wasOk()&&store.capacity()==502,"Sparse added presets expand the library with an empty destination");
        check(rig::slotCount==128&&board::pedalMidiTarget(0)==128&&board::targets==141,"Expanded preset storage does not shift historical MIDI pedal targets");
        board::State restored;
        check(store.load(500,{},identity,restored).wasOk()&&restored.presetId==state.presetId,"Expanded presets reload with their stable identity");
        const auto unicodeName=String::fromUTF8("Future preset — personal");
        check(store.rename(500,unicodeName).wasOk()&&store.readName(500)==unicodeName,"Expanded preset names and metadata support Unicode");
        const auto oldId=state.presetId;
        xml->setAttribute("minEngineVersion","9.0.0");
        check(board::decode(*xml,{},identity,state).failed()&&state.presetId==oldId,"Unsupported future engine versions fail without publishing partial state");
        xml->removeAttribute("minEngineVersion");xml->setAttribute("requiredFeatures","new-future-effect");
        check(board::decode(*xml,{},identity,state).failed(),"Unknown required sound capabilities are rejected explicitly");
        xml->removeAttribute("requiredFeatures");xml->setAttribute("version",18);
        check(board::decode(*xml,{},identity,state).failed(),"A future incompatible schema is not silently misinterpreted");
    }
    auto jamJSON=nlohmann::json::parse(resources.getChildFile("Jam/backing-tracks.json").loadFileAsString().toStdString());
    const auto jamFile=root.getChildFile("JamOverride.json");
    const auto firstTrack=jamJSON["tracks"][0];jamJSON["tracks"]=nlohmann::json::array();jamJSON["tracks"].push_back(firstTrack);jamFile.replaceWithText(String(jamJSON.dump()));
    jam::View jam;jam.setCatalogueFile(resources.getChildFile("Jam/backing-tracks.json"));jam.setCatalogueOverride(jamFile);
    if(jam.trackCount()!=1)report+="INFO JAM override error: "+jam.catalogueError()+"; JSON="+String(jamJSON.dump()).substring(0,400)+"\n";
    check(jam.trackCount()==1,"JAM loads an external catalogue override");
    jamFile.replaceWithText("{broken");jam.reload();
    check(jam.trackCount()==1,"A malformed JAM edit retains the last valid catalogue");
    jamFile.deleteFile();jam.reload();
    check(jam.trackCount()>1,"Removing the override restores the shipped JAM catalogue");
    report+="Audit 1.1.42 behaviour checks completed.\n";
    return ok;
}
}
