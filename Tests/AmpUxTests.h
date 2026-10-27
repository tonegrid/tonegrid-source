#pragma once
#include "NamAmpTests.h"
#include "AmpChassis.h"
#include "Effects/StereoSpread/SpreadControls.h"
namespace ampUxTests
{
inline bool run(const juce::File& models,juce::String& report)
{
    using namespace juce;bool ok=true;int checks=0;auto check=[&](bool pass,const String& name){++checks;ok&=pass;report+=String(pass?"PASS ":"FAIL ")+name+"\n";};
    const auto temp=File::getSpecialLocation(File::tempDirectory),root=temp.getChildFile("ToneGrid-amp-ux-"+Uuid().toString());
    ScopeGuard cleanup{[&]{if(root.isAChildOf(temp))root.deleteRecursively();}};namamp::Library library(root.getChildFile("NAM/Amps"));namamp::Catalog catalog;
    check(library.refresh(catalog).wasOk()&&catalog.entries.empty(),"Recursive NAM discovery uses a unique temporary root");
    auto put=[&](const String& relative){const auto file=library.folder().getChildFile(relative);file.getParentDirectory().createDirectory();file.replaceWithText(namTests::linear());};
    put("Root.nam");check(library.refresh(catalog).wasOk()&&catalog.entries.size()==1&&catalog.entries[0].identity.file=="Root.nam","Root-only NAM library retains historical identity");
    put("JM Continuum/Clean.nam");put("ToneJunkie/Deluxe/Clean.nam");check(library.refresh(catalog).wasOk()&&catalog.entries.size()==3,"Recursive discovery includes one-level and deeply nested subfolders");
    check(catalog.entries[0].identity.file=="JM Continuum/Clean.nam"&&catalog.entries[2].identity.file=="ToneJunkie/Deluxe/Clean.nam","Duplicate filenames are distinguished by full relative path and stable sorting");
    namamp::Entry entry;nlohmann::json payload;check(library.resolve(catalog.entries[2].identity,entry,payload).wasOk(),"Nested NAM identity resolves without absolute machine paths");
    check(namamp::folderLabel("Root.nam")=="Root"&&namamp::filename("Root.nam")=="Root.nam","Root label and exact extension are preserved");
    check(namamp::folderLabel("ToneJunkie/JM Continuum/Clean.nam")=="JM Continuum"&&namamp::filename("ToneJunkie/JM Continuum/Clean.nam")=="Clean.nam","Active NAM display uses immediate parent and exact actual filename");
    bool safe=true;for(const auto* path:{"/a.nam","../a.nam","A/../../a.nam","A//a.nam","A/./a.nam","C:/a.nam","A\\a.nam"})safe&=!namamp::validRelativePath(path);check(safe,"Recursive identities reject absolute, traversal and ambiguous separators");
    const auto saved=catalog.entries[2].identity;put("New/Added.nam");check(library.refresh(catalog).wasOk()&&catalog.entries.size()==4,"Recursive Refresh discovers a new folder/capture");
    library.folder().getChildFile(saved.file).deleteFile();check(library.resolve(saved,entry,payload).failed()&&library.refresh(catalog).wasOk()&&catalog.entries.size()==3,"Missing nested capture fails without filename-only substitution");
    namamp::State state{catalog.entries[0].identity,5,-8},decoded;check(namamp::decode(namamp::encode(state),decoded)&&decoded.model==state.model&&decoded.gainDb==5&&decoded.volumeDb==-8,"Existing NAM state format preserves full portable relative identity");
    tonegrid::ampui::Thumbnail thumbnail;thumbnail.setSize(220,130);check(thumbnail.usesFallback()&&thumbnail.createComponentSnapshot(thumbnail.getLocalBounds()).isValid(),"NAM thumbnail fallback is a rendered generic ToneGrid amp, never a broken image");
    Image art(Image::ARGB,10,10,true);thumbnail.setImage(art);check(!thumbnail.usesFallback(),"Thumbnail presentation accepts available artwork without fabricated metadata");
    tonegrid::SpreadControls controls;controls.sync({});check(!controls.state().spreadEnabled&&controls.state().offsetMs==15&&controls.state().wobblePercent==25&&controls.state().crossoverHz==130,"Advanced 3D popup preserves existing Spread defaults and parameters");
    auto* offset=dynamic_cast<Slider*>(controls.findChildWithID("spread-offset"));auto* enabled=dynamic_cast<Button*>(controls.findChildWithID("spread-enable"));auto* help=dynamic_cast<Label*>(controls.findChildWithID("spread-help"));
    offset->setValue(-10,sendNotificationSync);enabled->setToggleState(true,dontSendNotification);enabled->onClick();check(controls.state().offsetMs==-10&&controls.state().spreadEnabled,"Advanced 3D controls produce existing state, with no invented Width parameter");
    offset->setValue(-11,sendNotificationSync);check(help&&help->getText()==tonegrid::spreadHelp("spread-offset")&&help->getX()>offset->getRight(),"Two-column popup updates centralized contextual help on selection");
    const auto lookup=[](int p,int n){return p==1?String{}:"model-"+String(p)+"-"+String(n);};auto rig=board::migrations::initialState();PluginDescription plugin;plugin.name="Synthetic";plugin.pluginFormatName="VST3";plugin.uniqueId=1;plugin.version="1";rig.name="Missing-pedal test";rig.amp.append("state",5);rig.input={3,-55,true};rig.pedals[1].enabled=true;rig.pedals[2].enabled=true;
    auto xml=board::encode(rig,plugin,[](int p,int n){return "model-"+String(p)+"-"+String(n);});board::State recalled;
    check(board::decode(xml,plugin,lookup,recalled).wasOk()&&!recalled.pedals[1].enabled&&recalled.pedals[2].enabled&&recalled.input.gainDb==3&&recalled.amp==rig.amp,"Missing pedal capture bypasses only that pedal; rest of rig and amp state load");
    check(recalled.missingCaptureDiagnostics.size()==1&&recalled.missingCaptureDiagnostics[0].contains("TS9")&&recalled.missingCaptureDiagnostics[0].contains("model-1-"+String(rig.pedals[1].preset)),"Missing capture diagnostic identifies pedal and saved resource");
    xml.getChildByAttribute("id","1")->setAttribute("tone",99);check(board::decode(xml,plugin,lookup,recalled).failed(),"Missing resource policy does not weaken parameter validation");
    const auto copied=root.getChildFile("Pedals");check(models.copyDirectoryTo(copied),"Missing-pedal loading fixture is copied only into temporary storage");
    auto pedalCatalog=XmlDocument::parse(copied.getChildFile("catalog.xml"));const auto file=pedalCatalog->getChildByAttribute("id","1")->getFirstChildElement()->getStringAttribute("file");copied.getChildFile(file).deleteFile();board::Processor processor;
    check(processor.load(copied).wasOk()&&!processor.available(1,0)&&processor.available(2,0),"One absent model does not prevent remaining pedal models from loading");auto setting=board::defaults(1,0);setting.enabled=true;processor.set(1,setting);check(!processor.get(1).enabled,"Unavailable pedal cannot be re-enabled into a null NAM model");
    processor.prepareToPlay(48000,64);AudioBuffer<float> audio(2,64);MidiBuffer midi;for(int i=0;i<64;++i)audio.setSample(0,i,.125f);processor.processBlock(audio,midi);check(audio.getSample(0,32)==.125f&&audio.getSample(1,32)==.125f,"Unavailable pedal bypass remains sample-exact and stereo-safe");
    report+=String(checks)+" amplifier UX/resource checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
