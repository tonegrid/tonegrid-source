#pragma once
#include "NamBrowserLifetimeTests.h"
#include "Shared/UI/Library/CabinetBrowserView.h"

namespace regression140Tests
{
inline bool run(juce::String& report)
{
    using namespace juce;bool ok=true;int count=0;
    const auto check=[&](bool pass,const String& text){ok&=pass;++count;report+=String(pass?"PASS ":"FAIL ")+"140 "+text+"\n";};
    namBrowserLifetimeTests::MouseHost host;host.setSize(1000,300);host.mouse().focusForTest();
    tonegrid::libraryui::GearCard amp("Amp");host.addAndMakeVisible(amp);amp.setBounds(0,0,300,160);
    ComboBox normalize;normalize.addItem("OFF",1);normalize.addItem("Full",2);amp.setRows({{&amp.load},{&normalize}});
    bool menuOpen=false;amp.keepVisible=[&]{return menuOpen;};
    for(int i=0;i<30;++i)amp.advanceHover(true,1./60.);
    normalize.grabKeyboardFocus();check(normalize.hasKeyboardFocus(true),"Normalize fixture has real JUCE keyboard focus before reproducing the bug");
    normalize.setSelectedId(2,sendNotificationSync);
    amp.advanceHover(false,1./60.);check(amp.hoverOpacity()>0&&amp.hoverOpacity()<1,"Changing Normalize starts a soft fade on mouse-out despite retained focus");
    for(int i=0;i<30;++i)amp.advanceHover(false,1./60.);
    check(amp.hoverOpacity()==0&&!amp.actions.isVisible()&&normalize.getSelectedId()==2,"Normalize controls and scrim disappear without an outside-app click; value is preserved");
    menuOpen=true;for(int i=0;i<30;++i)amp.advanceHover(false,1./60.);
    check(amp.hoverOpacity()==1,"An open dropdown alone keeps its controls available while choosing an item");
    menuOpen=false;for(int i=0;i<30;++i)amp.advanceHover(false,1./60.);
    check(amp.hoverOpacity()==0,"Closing the dropdown outside the card clears the temporary menu hold");
    check(host.getComponentAt(Point<int>(20,12))==&amp.browse,"Faded controls leave the underlying artwork clickable");
    tonegrid::cabinet::Panel cab;host.addAndMakeVisible(cab);cab.setBounds(330,0,300,160);cab.update(nullptr,"Choose IR",true,false,true);
    for(int i=0;i<30;++i)cab.advanceHover(true,1./60.);
    cab.lock.grabKeyboardFocus();check(cab.lock.hasKeyboardFocus(true),"IR lock fixture has real JUCE keyboard focus before reproducing the bug");
    cab.lock.setToggleState(true,sendNotificationSync);for(int i=0;i<30;++i)cab.advanceHover(false,1./60.);
    check(cab.hoverOpacity()==0&&!cab.actions.isVisible()&&cab.lock.getToggleState(),"Lock and scrim disappear on mouse-out while retaining the IR lock");
    tonegrid::motion::CardFade a,b;float v60=0,v120=0;
    const auto first=a.advance(true,1./60.);v60=first;for(int i=1;i<6;++i)v60=a.advance(true,1./60.);for(int i=0;i<12;++i)v120=b.advance(true,1./120.);
    check(first>0&&first<.03f&&std::abs(v60-v120)<1e-5f,"Card ease starts gently and is independent of 60/120 Hz frame intervals");
    const auto turning=a.advance(false,0);check(std::abs(turning-v60)<1e-6f,"Reversing a partial fade preserves the current opacity");
    bool bounded=true;float last=turning;for(int i=0;i<30;++i){const auto next=a.advance(false,1./60.);bounded&=next>=0&&next<=last;last=next;}
    check(bounded&&last==0,"Fade-out is monotonic and settles exactly at zero");
    // Native window services are supplied by the windowless peer. Mouse routing,
    // button clicks, component focus and the parent DialogWindow are real JUCE.
    struct Dialog final:DialogWindow
    {
        Dialog():DialogWindow("Library test",Colours::black,true,false){}
        int closes=0;void closeButtonPressed() override{++closes;setVisible(false);}
    };
    const auto closeLibrary=[&](Component& browser,const String& id)
    {
        for(const int width:{760,1240})
        {
        Dialog dialog;namBrowserLifetimeTests::MouseHost mouseHost;mouseHost.setSize(1300,760);
        dialog.setContentNonOwned(&browser,false);mouseHost.addAndMakeVisible(dialog);dialog.setBounds(0,0,width,700);
        auto* button=dynamic_cast<Button*>(browser.findChildWithID(id));check(button&&button->getButtonText()=="OK",id+" is present with the requested caption");
        if(button)
        {
            const auto point=mouseHost.getLocalPoint(button,button->getLocalBounds().getCentre()).toFloat();
            check(button->getRight()<=browser.getWidth()&&button->getBottom()<=browser.getHeight(),id+" stays inside the browser footer");
            mouseHost.mouse().event(point,false);mouseHost.mouse().event(point,true);mouseHost.mouse().release();
            check(dialog.closes==1&&!dialog.isVisible(),id+" closes through the real parent dialog from a mouse click");
        }
        dialog.clearContentComponent();
        }
    };
    tonegrid::NamLibraryBrowser ampBrowser;closeLibrary(ampBrowser,"nam-library-ok");
    tonegrid::cabinet::Browser cabBrowser;closeLibrary(cabBrowser,"cabinet-ir-ok");
    report+=String(count)+" regression 1.1.40 checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
