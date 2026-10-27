#pragma once
#include "Shared/UI/Library/NamLibraryBrowser.h"
#include "Shared/UI/Library/CabinetBrowserView.h"

namespace libraryEscapeTests
{
inline bool run(juce::String& report)
{
    using namespace juce;
    bool passed=true;int checks=0;
    const auto check=[&](bool ok,const String& message){++checks;passed&=ok;report+=(ok?"PASS ":"FAIL ")+message+"\n";};
    Component owner;owner.setSize(1120,760);
    tonegrid::NamLibraryBrowser amps;tonegrid::cabinet::Browser cabinets;
    tonegrid::popup::Window ampWindow("Amps",amps,owner,false,440),cabWindow("Cabinets",cabinets,owner,false,440);
    ampWindow.setVisible(false);cabWindow.setVisible(false);
    std::function<TextEditor*(Component&,const String&)> find=[&](Component& c,const String& id)->TextEditor*{
        if(c.getComponentID()==id)return dynamic_cast<TextEditor*>(&c);
        for(auto* child:c.getChildren())if(auto* result=find(*child,id))return result;
        return nullptr;
    };
    auto* ampSearch=find(ampWindow,"nam-library-search");auto* cabSearch=find(cabWindow,"cabinet-ir-search");
    check(ampSearch!=nullptr,"Actual amp search editor is available in its popup header");
    check(cabSearch!=nullptr,"Actual cabinet search editor is available in its popup header");
    if(!ampSearch||!cabSearch)return false;
    struct Poll final:Timer{std::function<void()> step;void timerCallback()override{if(step)step();}} poll;
    int phase=0;bool waiting=false;double deadline=0;
    poll.step=[&]{
        auto& window=phase<2?ampWindow:cabWindow;auto* search=phase<2?ampSearch:cabSearch;
        if(!waiting){
            window.setVisible(true);search->grabKeyboardFocus();search->setText(phase%2==0?"Marshall Greenback":"",false);
            deadline=Time::getMillisecondCounterHiRes()+2000.;waiting=true;
            // Exercise TextEditor's real asynchronous Escape path; do not call
            // the dialog close action directly. Physical dispatch is tested too.
            search->keyPressed(KeyPress(KeyPress::escapeKey));return;
        }
        if(window.isVisible()&&Time::getMillisecondCounterHiRes()<deadline)return;
        check(!window.isVisible(),String(phase<2?"Amp":"Cabinet")+" popup closes on Escape from "+(phase%2==0?"populated":"empty")+" search");
        window.setVisible(false);waiting=false;
        if(++phase==4){poll.stopTimer();MessageManager::getInstance()->stopDispatchLoop();}
    };
    poll.startTimer(10);MessageManager::getInstance()->runDispatchLoop();poll.stopTimer();
    report+=String(checks)+" library Escape checks; "+(passed?"ALL PASSED":"FAILURES")+"\n";return passed;
}
}
