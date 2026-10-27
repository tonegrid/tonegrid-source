#pragma once
#include "PresetBrowser.h"
#include "Shared/UI/PopupWindow.h"
#include "Content/Amps/FactoryAmps.h"

namespace regression139Tests
{
inline bool run(juce::String& report)
{
    using namespace juce;bool ok=true;int checks=0;
    const auto check=[&](bool pass,const String& label){++checks;ok&=pass;report+=String(pass?"PASS ":"FAIL ")+"139 "+label+"\n";};
    Component owner;owner.setSize(1360,820);
    tonegrid::PresetBrowser browser;browser.showToneBank(true);
    tonegrid::popup::Window window("Presets",browser,owner,false,440);window.setVisible(true);
    auto* search=dynamic_cast<TextEditor*>(browser.findChildWithID("tonebank-search"));
    check(search!=nullptr,"preset browser exposes its real search editor");
    if(search)
    {
        search->grabKeyboardFocus();search->setText("Little Wing",false);
        check(window.isVisible(),"focused search starts in an open preset window");
        // Exercise TextEditor's own key path, which consumed Escape in 1.1.38.
        // Physical OS dispatch is checked separately in the native preview.
        struct Poll final:Timer
        {
            std::function<void()> action;
            void timerCallback() override {if(action)action();}
        } poll;
        const auto deadline=Time::getMillisecondCounterHiRes()+2000.;int phase=0;
        poll.action=[&]
        {
            if(window.isVisible()&&Time::getMillisecondCounterHiRes()<deadline)return;
            const bool closed=!window.isVisible();
            check(closed,phase==0?"Escape consumed by TextEditor closes its containing preset window"
                                  :"Escape also closes an empty search field after reopening");
            if(phase++==0&&closed)
            {
                window.setVisible(true);search->setText({},false);
                search->keyPressed(KeyPress(KeyPress::escapeKey));return;
            }
            poll.stopTimer();MessageManager::getInstance()->stopDispatchLoop();
        };
        search->keyPressed(KeyPress(KeyPress::escapeKey));poll.startTimer(10);
        // TextEditor posts its Escape callback asynchronously. This is the
        // runner's top-level event loop, not a nested modal/message pump.
        MessageManager::getInstance()->runDispatchLoop();
    }
    const auto temp=File::getSpecialLocation(File::tempDirectory),root=temp.getChildFile("ToneGrid-relocation-"+Uuid().toString());
    ScopeGuard cleanup{[&]{if(root.isAChildOf(temp))root.deleteRecursively();}};
    const auto bundle=root.getChildFile("Bundle"),libraryRoot=root.getChildFile("Library");
    const String oldName="06 - Head Packs/Test/Head.nam",newName="01 - Clean/Test - Head Only/Head.nam";
    const auto old=libraryRoot.getChildFile(oldName),current=bundle.getChildFile(newName);
    old.getParentDirectory().createDirectory();current.getParentDirectory().createDirectory();
    old.replaceWithText(R"({"version":"0.7.0","architecture":"Linear","sample_rate":48000,"config":{"receptive_field":1,"bias":false},"weights":[1]})");old.copyFileTo(current);
    namamp::Library library(libraryRoot);namamp::Entry before,resolved;nlohmann::json payload;
    check(library.read(old,before).wasOk(),"saved Head identity exists before the factory move");
    tonegrid::libraryui::Favorites favorites;const auto favoriteFile=root.getChildFile("favorites.xml");favorites.load(favoriteFile);favorites.toggle(old);const auto savedFavorites=favoriteFile.loadFileAsString();
    const auto hash=before.identity.sha256.toStdString();
    const nlohmann::json moves={{"schemaVersion",1},{"moves",nlohmann::json::array({{{"from",oldName.toStdString()},{"to",newName.toStdString()},{"sha256",hash},{"bytes",before.identity.bytes}}})}};
    const auto map=bundle.getChildFile("factory-relocations.json");map.replaceWithText(String(moves.dump()));
    const nlohmann::json manifest={{"schemaVersion",2},{"ampCount",1},{"amps",nlohmann::json::array({{{"filename",newName.toStdString()},{"sha256",hash}}})},{"assets",nlohmann::json::array({{{"filename","factory-relocations.json"},{"sha256",SHA256(map).toHexString().toStdString()}}})},{"retiredAmps",nlohmann::json::array({{{"filename",oldName.toStdString()},{"sha256",hash}}})}};
    bundle.getChildFile("manifest.json").replaceWithText(String(manifest.dump()));std::atomic<bool> cancel{false};
    check(tonegrid::factory::seed(bundle,libraryRoot,cancel).wasOk()&&!old.existsAsFile(),"factory update installs the new category and retires only the exact old capture");
    check(library.resolve(before.identity,resolved,payload).wasOk()&&resolved.identity.file==newName,"old preset resolves its moved Head with identical bytes");
    favorites.followFactoryMoves(libraryRoot);
    check(favorites.contains(libraryRoot.getChildFile(newName))&&favoriteFile.loadFileAsString()==savedFavorites,"favorites follow exact factory moves without rewriting the personal favorites file");
    const auto destination=libraryRoot.getChildFile(newName);destination.appendText(" ");
    check(library.resolve(before.identity,resolved,payload).failed(),"changed bytes at a relocated Head are rejected");
    old.getParentDirectory().createDirectory();old.replaceWithText("personal capture");
    check(library.resolve(before.identity,resolved,payload).failed()&&old.loadFileAsString()=="personal capture","relocation never overrides an existing modified personal capture");
    report+=String(checks)+" ToneGrid 1.1.39 keyboard and preset relocation checks\n";return ok;
}
}
