#pragma once
#include "CabinetProcessor.h"
#include "Content/Cabinets/CabinetBrowser.h"
#include "Shared/UI/Library/CabinetBrowserView.h"
#include "CaptureKind.h"
#include "Content/Presets/RigCodec.h"
#include "RealtimeAllocationProbe.h"

namespace cabinetIRTests
{
inline bool run(juce::String& report)
{
    using namespace juce;using namespace tonegrid::cabinet;bool ok=true;int checks=0;
    auto check=[&](bool value,const String& name){++checks;ok&=value;report+=(value?"PASS ":"FAIL ")+name+"\n";};
    check(kind("amp_cab")==CaptureKind::fullRig&&kind("amp-cab")==CaptureKind::fullRig&&kind("full-rig")==CaptureKind::fullRig,"Explicit full-rig metadata bypasses external cabinet");
    check(kind("amp")==CaptureKind::head&&kind("amp_head")==CaptureKind::head&&kind("preamp")==CaptureKind::preamp&&kind("mystery")==CaptureKind::unknown,"Head, preamp and unknown types stay distinct");
    const auto root=tonegrid::paths::temporaryRoot().getChildFile("ToneGrid-cab-test-"+Uuid().toString());root.createDirectory();
    const ScopeGuard cleanup{[&]{if(root.isAChildOf(tonegrid::paths::temporaryRoot()))root.deleteRecursively();}};
    const auto wav=root.getChildFile("test.wav");
    {
        AudioBuffer<float> impulse(1,32);impulse.clear();impulse.setSample(0,0,1.f);impulse.setSample(0,2,.5f);
        WavAudioFormat format;auto stream=wav.createOutputStream();std::unique_ptr<AudioFormatWriter> writer(format.createWriterFor(stream.release(),48000,1,24,{},0));
        check(writer&&writer->writeFromAudioSampleBuffer(impulse,0,32),"Create a known mono test cabinet");
    }
    {
        const auto shipped=root.getChildFile("FactoryCabinetIR"),user=root.getChildFile("existing-profile/CabinetIR");const String relative="Factory/4x12/Test Pack/test.wav";
        const auto target=shipped.getChildFile(relative);target.getParentDirectory().createDirectory();wav.copyFileTo(target);
        Image image(Image::RGB,8,8,true);image.clear(image.getBounds(),Colours::red);PNGImageFormat png;const auto picture=target.getSiblingFile("thumbnail.png");{auto out=picture.createOutputStream();png.writeImageToStream(image,*out);}
        nlohmann::json catalog={{"defaultIR",relative.toStdString()},{"irs",nlohmann::json::array({{{"file",relative.toStdString()},{"sha256",SHA256(target).toHexString().toStdString()},{"pack","Test Pack"},{"image","Factory/4x12/Test Pack/thumbnail.png"}}})},{"assets",nlohmann::json::array({{{"file","Factory/4x12/Test Pack/thumbnail.png"},{"sha256",SHA256(picture).toHexString().toStdString()}}})},{"recommendations",nlohmann::json::object()}};
        shipped.getChildFile("catalog.json").replaceWithText(String(catalog.dump()));Library library(user,shipped);library.scan(true);
        check(library.fallback()&&library.fallback()->state.file==relative&&library.fallback()->state.sha256==SHA256(wav).toHexString()&&user.getChildFile("Factory/4x12/Test Pack/thumbnail.png").existsAsFile(),"Renamed factory root seeds pack-local artwork and preserves relative IR identity");
        image.clear(image.getBounds(),Colours::blue);const auto custom=user.getChildFile("Factory/4x12/Test Pack/thumbnail.jpg");{JPEGImageFormat jpg;auto out=custom.createOutputStream();jpg.writeImageToStream(image,*out);}
        const auto personal=SHA256(custom).toHexString();library.scan(true);
        check(library.fallback()&&library.fallback()->image.getPixelAt(3,3).getBlue()>200&&SHA256(custom).toHexString()==personal,"Shared amp/cab thumbnail.jpg precedence preserves personal artwork on reseed");
        const auto ancestor=root.getChildFile("Inherited/Old/Pack"),newFolder=root.getChildFile("Inherited/New/Pack");ancestor.createDirectory();
        const auto parentImage=ancestor.getParentDirectory().getChildFile("thumbnail.png");{auto out=parentImage.createOutputStream();png.writeImageToStream(image,*out);}
        retainCabinetArtwork(ancestor,newFolder,root);check(newFolder.getChildFile("thumbnail.png").existsAsFile()&&SHA256(parentImage).toHexString()==SHA256(newFolder.getChildFile("thumbnail.png")).toHexString(),"Migration retains a thumbnail inherited from a physical parent folder");
        const auto specific=user.getChildFile(relative).withFileExtension("png");image.clear(image.getBounds(),Colours::red);{auto out=specific.createOutputStream();png.writeImageToStream(image,*out);}library.scan(false);
        check(library.fallback()&&library.fallback()->image.getPixelAt(3,3).getRed()>200,"Capture-specific IR artwork takes precedence over the pack thumbnail, matching NAM");image.clear(image.getBounds(),Colours::blue);
        const auto loose=user.getChildFile("My cabinets/2x12 Personal.wav");loose.getParentDirectory().createDirectory();wav.copyFileTo(loose);{auto out=loose.getSiblingFile("thumbnail.png").createOutputStream();png.writeImageToStream(image,*out);}library.scan(false);
        const auto* imported=library.find("My cabinets/2x12 Personal.wav");check(imported&&imported->image.getPixelAt(2,2).getBlue()>200&&imported->category=="2x12","Open Folder plus Refresh discovers a personal WAV and sibling thumbnail without a sidecar");
    }
    {
        const auto shipped=root.getChildFile("PhysicalBundle"),user=root.getChildFile("PhysicalLibrary");
        const String previous="Factory/4x12/1234/test.wav",current="4x12/Named Cabinet/test.wav";
        const auto stock=shipped.getChildFile(current);stock.getParentDirectory().createDirectory();wav.copyFileTo(stock);
        const auto old=user.getChildFile(previous);old.getParentDirectory().createDirectory();wav.copyFileTo(old);const auto hash=SHA256(old).toHexString();
        Image blue(Image::RGB,8,8,true);blue.clear(blue.getBounds(),Colours::blue);PNGImageFormat png;
        const auto custom=old.getSiblingFile("thumbnail.png");{auto out=custom.createOutputStream();png.writeImageToStream(blue,*out);}const auto artHash=SHA256(custom).toHexString();
        const auto personal=user.getChildFile(String::fromUTF8("Factory/412 ToneGrid/01 Balanced/Cabinet personnel é.wav"));personal.getParentDirectory().createDirectory();wav.copyFileTo(personal);
        const auto conflicting=user.getChildFile("Factory/Keep/My.wav"),destination=user.getChildFile("Keep/My.wav");conflicting.getParentDirectory().createDirectory();wav.copyFileTo(conflicting);destination.getParentDirectory().createDirectory();destination.replaceWithText("personal conflict must survive");
        const auto favorite=user.getChildFile("favorites.xml");XmlElement xml("CabinetFavorites");xml.createNewChildElement("IR")->setAttribute("sha256",hash);xml.writeTo(favorite);
        nlohmann::json catalog={{"physicalFolders",true},{"defaultIR",current.toStdString()},{"irs",nlohmann::json::array({{{"file",current.toStdString()},{"legacyFile",previous.toStdString()},{"sha256",hash.toStdString()},{"category","4x12"},{"pack","Metadata is not a directory"}}})},{"assets",nlohmann::json::array()},{"recommendations",{{"head-identity",current.toStdString()}}}};
        shipped.getChildFile("catalog.json").replaceWithText(String(catalog.dump()));Library library(user,shipped);library.scan(true);
        check(user.getChildFile(current).existsAsFile()&&!old.exists()&&library.fallback()&&library.fallback()->state.sha256==hash,"Physical migration preserves factory audio bytes and the default cabinet");
        check(library.recommended("head-identity")&&library.recommended("head-identity")->state.file==current&&library.find(previous)&&library.find(previous)->state.file==current,"Existing head recommendations and legacy cabinet paths resolve the same IR");
        check(SHA256(user.getChildFile("4x12/Named Cabinet/thumbnail.png")).toHexString()==artHash&&library.fallback()->image.getPixelAt(3,3).getBlue()>200,"Personal thumbnail survives the cabinet-folder migration");
        const auto moved=user.getChildFile(String::fromUTF8("412 ToneGrid/01 Balanced/Cabinet personnel é.wav"));
        check(moved.existsAsFile()&&SHA256(moved).toHexString()==hash&&!personal.exists(),"Personal nested IR folders and Unicode filenames survive unwrapping the former Factory directory");
        check(conflicting.existsAsFile()&&destination.loadFileAsString()=="personal conflict must survive","Conflicting personal destinations are never overwritten or discarded");
        const auto favoriteHash=SHA256(favorite).toHexString();library.scan(true);check(SHA256(favorite).toHexString()==favoriteHash&&library.find(current),"Repeated migration is idempotent and leaves hash-based favorites untouched");
        Browser browser;browser.setSize(1240,640);browser.update(library,{},favorite);
        check(browser.sidebarFolders()==4,"Sidebar derives real folders including a preserved collision folder, never hard-coded cabinet sizes");
        browser.navigate("412 ToneGrid");check(browser.visibleRows()==1&&browser.rowPath(0)=="412 ToneGrid/01 Balanced","Personal folders appear as navigable physical subdirectories");
        browser.navigate("412 ToneGrid/01 Balanced");check(browser.visibleRows()==1&&browser.rowPath(0).endsWith(String::fromUTF8("Cabinet personnel é.wav")),"Folder without a sidecar or speaker count in its filename displays its WAV");
        browser.navigate("4x12");check(browser.visibleRows()==1&&browser.rowPath(0)=="4x12/Named Cabinet","Cabinet name comes from its folder rather than a metadata pack title");
        browser.navigate("4x12/Named Cabinet");check(browser.visibleRows()==1&&browser.rowPath(0)==current,"Physical pack opens the WAV files inside it");
        browser.showFavorites();check(browser.visibleRows()>=2,"Existing favorites still select the identical IR content after migration");
        State oldState{previous,hash,true};auto restored=prepare(user.getChildFile(previous),user,&oldState);
        check(restored.result.wasOk()&&restored.state.sha256==hash&&restored.state.locked,"Saved legacy preset can load the exact WAV and retains the IR lock");
        const auto renamed=user.getChildFile("Custom sizes/Deep/Renamed.wav");renamed.getParentDirectory().createDirectory();moved.moveFileTo(renamed);library.scan(false);browser.update(library,{},favorite);
        browser.navigate("Custom sizes");check(browser.visibleRows()==1&&browser.rowPath(0)=="Custom sizes/Deep","Refresh discovers an arbitrary added folder without inferring a virtual category");
        browser.navigate("Custom sizes/Deep");check(browser.visibleRows()==1&&browser.rowPath(0)=="Custom sizes/Deep/Renamed.wav","Refresh discovers renamed WAV files in nested physical folders");
        browser.navigate("412 ToneGrid/01 Balanced");check(browser.visibleRows()==0,"Refresh removes a moved file from its old folder");
        Image red(Image::RGB,8,8,true);red.clear(red.getBounds(),Colours::red);const auto newThumb=renamed.getSiblingFile("thumbnail.png");{auto out=newThumb.createOutputStream();png.writeImageToStream(red,*out);}library.scan(false);
        check(library.find("Custom sizes/Deep/Renamed.wav")&&library.find("Custom sizes/Deep/Renamed.wav")->image.getPixelAt(2,2).getRed()>200,"Refresh reads a newly added local thumbnail");
        newThumb.deleteFile();library.scan(false);check(!library.folderImages.contains("Custom sizes/Deep"),"Refresh removes a deleted thumbnail from the folder cache");
        browser.update(library,{},favorite);browser.navigate("Custom sizes/Deep");renamed.deleteFile();library.scan(false);browser.update(library,{},favorite);check(browser.visibleRows()==0,"Refresh removes deleted WAVs without reseeding them");
        const auto stockHash=SHA256(user.getChildFile(current)).toHexString();library.scan(false);check(SHA256(user.getChildFile(current)).toHexString()==stockHash,"Ordinary Refresh never changes IR audio bytes");
        const auto later=user.getChildFile("Factory/My new physical folder/Later.wav");later.getParentDirectory().createDirectory();wav.copyFileTo(later);library.scan(true);
        check(later.existsAsFile()&&!user.getChildFile("My new physical folder/Later.wav").exists(),"Legacy-wrapper migration runs once and respects a Factory folder created by the user afterwards");
        const auto replacement=user.getChildFile(current);replacement.appendText("personal IR edit");const auto replacementHash=SHA256(replacement).toHexString();library.scan(false);
        check(library.find(current)&&library.find(current)->state.sha256==replacementHash,"A personal replacement of a stock WAV remains discoverable at its physical path");
        const auto* associated=library.recommended("head-identity");check(associated&&associated->state.sha256==hash&&associated->state.sha256!=replacementHash,"Recommendations resolve the original exact IR instead of silently using a changed personal replacement");
    }
    auto prepared=prepare(wav,root);check(prepared.result.wasOk(),"Decode and normalize a valid WAV off the audio thread");
    auto saved=prepared.state;saved.locked=true;State decoded;
    check(decode(encode(saved),decoded)&&decoded.file==saved.file&&decoded.sha256==saved.sha256&&decoded.locked,"IR identity and lock round-trip");
    auto invalid=saved;invalid.file="../production.wav";check(!valid(invalid),"Reject path traversal");invalid=saved;invalid.sha256=String::repeatedString("0",64);
    check(prepare(wav,root,&invalid).result.failed(),"Changed cabinet rejected before publishing sound state");
    check(prepare(root.getChildFile("missing.wav"),root).result.failed(),"Missing cabinet rejected");
    Node processor;processor.prepareToPlay(48000,128);AudioBuffer<float> signal(2,128);MidiBuffer midi;
    signal.clear();signal.setSample(0,0,.25f);signal.setSample(1,2,-.125f);processor.enable(false);processor.processBlock(signal,midi);
    check(signal.getSample(0,0)==.25f&&signal.getSample(1,2)==-.125f,"Full-rig / VST bypass is sample-identical");
    processor.enable(true);processor.processBlock(signal,midi);check(signal.getMagnitude(0,128)==0.f,"An unloaded head cabinet cannot emit an unfiltered signal");
    auto good=prepare(wav,root);processor.load(std::move(good));processor.prepareToPlay(48000,128);processor.enable(true);
    signal.clear();signal.setSample(0,0,.25f);signal.setSample(1,0,.25f);processor.processBlock(signal,midi);
    check(std::abs(signal.getSample(0,0))>1e-5f&&std::abs(signal.getSample(0,2)/signal.getSample(0,0)-.5f)<.002f,"Head output passes through the selected cabinet convolution");
    check(std::abs(signal.getSample(0,2)-signal.getSample(1,2))<1e-6f,"Mono cabinet renders both stereo channels consistently");
    signal.clear();processor.processBlock(signal,midi);realtimeProbe::begin();
    for(int block=0;block<64;++block){signal.clear();processor.processBlock(signal,midi);}
    const auto allocations=realtimeProbe::end();check(allocations==0,"Cabinet callback performs no C++ heap allocations after preparation");
    for(double rate:{44100.,96000.,192000.})
    {
        const auto other=root.getChildFile("rate-"+String(int(rate))+".wav");
        {AudioBuffer<float> impulse(2,64);impulse.clear();impulse.setSample(0,0,1.f);impulse.setSample(1,0,.5f);WavAudioFormat format;auto stream=other.createOutputStream();std::unique_ptr<AudioFormatWriter> writer(format.createWriterFor(stream.release(),rate,2,24,{},0));if(writer)writer->writeFromAudioSampleBuffer(impulse,0,64);}
        auto ir=prepare(other,root);check(ir.result.wasOk(),"Accept stereo IR at "+String(int(rate))+" Hz");
        if(ir.result.wasOk()){Node resampled;resampled.load(std::move(ir));resampled.prepareToPlay(48000,128);resampled.enable(true);signal.clear();signal.setSample(0,0,.25f);signal.setSample(1,0,.25f);resampled.processBlock(signal,midi);bool finite=true;for(int c=0;c<2;++c)for(int i=0;i<128;++i)finite&=std::isfinite(signal.getSample(c,i));check(finite&&signal.getMagnitude(0,128)>0,"Resample IR to the 48 kHz engine at "+String(int(rate))+" Hz");}
    }
    for(int frames:{96000,96001})
    {
        const auto longFile=root.getChildFile("long-"+String(frames)+".wav");
        {AudioBuffer<float> impulse(1,frames);impulse.clear();impulse.setSample(0,0,.5f);WavAudioFormat format;auto stream=longFile.createOutputStream();std::unique_ptr<AudioFormatWriter> writer(format.createWriterFor(stream.release(),48000,1,24,{},0));if(writer)writer->writeFromAudioSampleBuffer(impulse,0,frames);}
        const auto ir=prepare(longFile,root);check(frames==96000?ir.result.wasOk():ir.result.failed(),frames==96000?"Accept complete two-second cabinet IRs without truncating the pack":"Reject cabinet IRs above the bounded two-second limit");
    }
    auto state=board::migrations::initialState();state.name="IR round trip";state.sourceType=amp::SourceType::Nam;
    state.namAmp.model={"test.nam",String::repeatedString("a",64),100};state.cabinet=saved;state.outputDsp=tonegrid::output::freshSessionSettings();
    const board::RigIdentityLookup identity=[](int p,int n){return "synthetic-"+String(p)+"-"+String(n);};
    auto xml=board::encode(state,{},identity);board::State restored;
    const auto restoredResult=board::decode(xml,{},identity,restored);
    check(xml.getIntAttribute("version")==17&&restoredResult.wasOk()&&restored.cabinet&&restored.cabinet->sha256==saved.sha256&&restored.cabinet->locked,"Preset v17 recalls head, cabinet identity and lock together");
    xml.addChildElement(new XmlElement(encode(saved)));check(board::decode(xml,{},identity,restored).failed(),"Duplicate cabinet state rejected");
    report+=String(checks)+" cabinet IR checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
