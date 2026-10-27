#pragma once
#include "NamBrowserLifetimeTests.h"
#include "Shared/UI/Library/CabinetBrowserView.h"
#include "Modules/Play/UI/PedalCard.h"
namespace libraryFeedback142Tests {
inline bool run(const juce::File& resources,juce::String& report) {
    using namespace juce;using namespace tonegrid;bool ok=true;int checks=0;
    const auto check=[&](bool pass,const String& text){++checks;ok&=pass;report+=String(pass?"PASS ":"FAIL ")+text+"\n";};
    const auto root=File::getSpecialLocation(File::tempDirectory).getChildFile("ToneGrid-feedback142-"+Uuid().toString());root.createDirectory();
    struct Cleanup{File path;~Cleanup(){path.deleteRecursively();}} cleanup{root};
    const auto wav=[&](const File& file,float value){file.getParentDirectory().createDirectory();AudioBuffer<float> impulse(1,32);impulse.clear();impulse.setSample(0,0,value);WavAudioFormat format;auto stream=file.createOutputStream();std::unique_ptr<AudioFormatWriter> writer(format.createWriterFor(stream.release(),48000,1,24,{},0));return writer&&writer->writeFromAudioSampleBuffer(impulse,0,32);};
    const auto bundle=root.getChildFile("Bundle"),user=root.getChildFile("User");
    const String target="4x12/01 Balanced/Normalized.wav",old="412 ToneGrid Normalized/01 Balanced/Normalized.wav",raw="412 ToneGrid/01 Balanced/Original.wav";
    check(wav(bundle.getChildFile(target),.5f)&&wav(user.getChildFile(raw),.25f),"Build isolated normalized/original IR fixtures");
    user.getChildFile(old).getParentDirectory().createDirectory();bundle.getChildFile(target).copyFileTo(user.getChildFile(old));
    const auto rawHash=SHA256(user.getChildFile(raw)).toHexString(),normalizedHash=SHA256(bundle.getChildFile(target)).toHexString();
    nlohmann::json catalog={{"defaultIR",target.toStdString()},{"irs",nlohmann::json::array({{{"file",target.toStdString()},{"sha256",normalizedHash.toStdString()},{"legacyFile",old.toStdString()},{"moveOnInstall",true},{"pack","ToneGrid Balanced"}}})},{"assets",nlohmann::json::array()},{"recommendations",{{std::string(64,'a'),target.toStdString()}}},{"retiredCollections",nlohmann::json::array({{{"folder","412 ToneGrid"},{"files",nlohmann::json::array({{{"file","01 Balanced/Original.wav"},{"sha256",rawHash.toStdString()}}})}}})}};
    bundle.getChildFile("catalog.json").replaceWithText(String(catalog.dump()));
    user.getChildFile(".tonegrid-cabinet-folders-v1").replaceWithText("1");
    cabinet::Library library(user,bundle);library.scan(true);
    check(user.getChildFile(target).existsAsFile()&&!user.getChildFile(old).exists()&&!user.getChildFile("412 ToneGrid Normalized").exists(),"Existing-profile install moves normalized packs under physical 4x12 even after the previous migration");
    const auto retired=user.getChildFile(".tonegrid-retired/"+raw);
    check(!user.getChildFile(raw).exists()&&retired.existsAsFile()&&SHA256(retired).toHexString()==rawHash,"Only declared original WAVs are retired with exact bytes retained");
    check(library.entries.size()==1&&!library.folders.contains(".tonegrid-retired")&&library.fallback()&&library.recommended(String::repeatedString("a",64)),"Retired captures stay out of browsing while default/recommended identities stay intact");
    const cabinet::State saved{raw,rawHash,true};const auto recovered=cabinet::prepare(user.getChildFile(raw),user,&saved);
    check(recovered.result.wasOk()&&recovered.state.sha256==rawHash&&recovered.state.locked,"An old locked preset still loads its original retired IR by hash");
    const auto moved=cabinet::State{old,normalizedHash,false};check(library.find(moved)&&library.find(moved)->state.file==target,"Saved normalized selections follow the declared move without changing sound identity");
    const auto scans=library.scans;library.scan(false);check(library.scans==scans+1&&library.entries.size()==1&&retired.existsAsFile(),"Refresh rebuilds the physical snapshot without restoring the removed collection");
    const auto custom=root.getChildFile("Personal");check(wav(custom.getChildFile(raw),.75f),"Create a changed personal original");cabinet::retireCabinetCollections(custom,catalog["retiredCollections"]);
    check(custom.getChildFile(raw).existsAsFile()&&!custom.getChildFile(".tonegrid-retired").exists(),"Modified personal WAVs are never retired by the stock migration");
    board::Processor pedals;PedalCard pedal(0,pedals);auto* selector=dynamic_cast<ComboBox*>(pedal.findChildWithID("pedal-preset"));
    check(selector&&selector->getTooltip().isEmpty(),"Pedal capture selector no longer displays the large metadata tooltip");
    const auto factory=nlohmann::json::parse(resources.getChildFile("FactoryCabinetIR/catalog.json").loadFileAsString().toStdString());int additions=0;bool exact=true;
    for(const auto& item:factory.at("irs"))if(item.value("collection",std::string{})=="tonegrid-normalized-412") {++additions;const String path(item.at("file").get<std::string>());const auto file=resources.getChildFile("FactoryCabinetIR").getChildFile(path);exact&=path.startsWith("4x12/")&&file.existsAsFile()&&SHA256(file).toHexString()==String(item.at("sha256").get<std::string>())&&cabinet::prepare(file,resources.getChildFile("FactoryCabinetIR")).result.wasOk();}
    check(additions==15&&exact,"All 15 retained normalized factory WAVs preserve their supplied hashes after the publisher's model-folder reorganisation");
    struct Dialog final:DialogWindow{Dialog():DialogWindow("Library",Colours::black,true,false){}int closes=0;void closeButtonPressed()override{++closes;setVisible(false);}} ampWindow,cabWindow;
    namBrowserLifetimeTests::MouseHost host;host.setSize(1300,760);host.addAndMakeVisible(ampWindow);host.addAndMakeVisible(cabWindow);
    NamLibraryBrowser amps;cabinet::Browser cabs;ampWindow.setContentNonOwned(&amps,false);cabWindow.setContentNonOwned(&cabs,false);ampWindow.setBounds(0,0,1240,700);cabWindow.setBounds(0,0,1240,700);
    for(auto pair:{std::pair<Component*,String>{&amps,"nam-library-ok"},{&cabs,"cabinet-ir-ok"}}){auto* button=pair.first->findChildWithID(pair.second);const auto pixel=button?button->createComponentSnapshot(button->getLocalBounds()).getPixelAt(12,20):Colours::black;check(button&&pixel.getBlue()>220&&pixel.getRed()<40,"Shared "+pair.second+" action uses the established ToneGrid blue");}
    const auto namRoot=root.getChildFile("Amps");namRoot.createDirectory();for(const auto* name:{"A.nam","B.nam"})namRoot.getChildFile(name).replaceWithText(R"({"version":"0.7.0","architecture":"Linear","sample_rate":48000,"config":{"receptive_field":1,"bias":false},"weights":[1]})");
    namamp::Catalog ampCatalog;namamp::Library(namRoot).refresh(ampCatalog);amps.update(ampCatalog,namRoot,{});amps.onSelect=[](const String&){};amps.onClose=[&]{ampWindow.closeButtonPressed();};
    check(wav(user.getChildFile("4x12/01 Balanced/Other.wav"),.4f),"Create a second cabinet selection fixture");library.scan(false);cabs.update(library,{},root.getChildFile("favorites.xml"));cabs.navigate("4x12/01 Balanced");
    std::function<Component*(Component&,const String&)> findName=[&](Component& c,const String& name)->Component*{if(c.getName()==name)return &c;for(auto* child:c.getChildren())if(auto* found=findName(*child,name))return found;return nullptr;};
    const auto doubleClick=[&](const String& name){if(auto* row=findName(cabs,name)){const auto point=row->getLocalBounds().getCentre().toFloat();row->mouseDoubleClick(MouseEvent(Desktop::getInstance().getMainMouseSource(),point,ModifierKeys(),1,0,0,0,0,row,row,Time::getCurrentTime(),point,Time::getCurrentTime(),2,false));return true;}return false;};
    int loads=0;cabs.onSelect=[&](const cabinet::State&){++loads;cabs.setSelectionEnabled(false);};
    struct Poll final:Timer{std::function<void()> step;void timerCallback()override{step();}} poll;int phase=0;
    poll.step=[&]{switch(phase++){
        case 0:amps.activate("A.nam",true);amps.handleUpdateNowIfNeeded();break;
        case 1:check(ampWindow.closes==0,"Double-clicking an amp waits for successful activation");amps.selectActive("A.nam");break;
        case 2:check(ampWindow.closes==1,"Amp popup closes after its asynchronous load succeeds");ampWindow.setVisible(true);amps.activate("B.nam",true);amps.handleUpdateNowIfNeeded();break;
        case 3:check(ampWindow.closes==1&&ampWindow.isVisible(),"An unsuccessful amp load keeps the browser open");if(auto* row=dynamic_cast<Button*>(findName(cabs,"Normalized")))row->onClick();else check(false,"Actual cabinet row is available");break;
        case 4:check(loads==1&&!cabs.canSelect(),"Single-click starts the actual cabinet row's asynchronous load");check(doubleClick("Normalized"),"Actual cabinet row accepts a double-click while its first load is pending");break;
        case 5:check(cabWindow.closes==0,"Cabinet double-click does not close before successful load confirmation");cabs.selectActive(target);cabs.setSelectionEnabled(true);break;
        case 6:check(cabWindow.closes==1,"Cabinet popup closes after loading the double-clicked IR");cabWindow.setVisible(true);check(doubleClick("Normalized"),"An already loaded cabinet remains double-clickable");break;
        case 7:check(cabWindow.closes==2&&loads==1,"Double-clicking the active IR closes without a redundant load");cabWindow.setVisible(true);check(doubleClick("Other"),"A different cabinet can be requested");break;
        case 8:check(cabWindow.closes==2&&loads==2,"A pending different IR keeps its popup visible");cabs.setSelectionEnabled(true,"Synthetic load failure");break;
        case 9:check(cabWindow.isVisible()&&cabWindow.closes==2,"A failed cabinet load keeps the popup open and clears automatic closure");cabs.selectActive("4x12/01 Balanced/Other.wav");break;
        case 10:check(cabWindow.closes==2,"A stale cabinet acknowledgement cannot close a failed selection");poll.stopTimer();MessageManager::getInstance()->stopDispatchLoop();break;
    }};poll.startTimer(30);MessageManager::getInstance()->runDispatchLoop();poll.stopTimer();ampWindow.clearContentComponent();cabWindow.clearContentComponent();
    report+=String(checks)+" library feedback 1.1.42 checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
