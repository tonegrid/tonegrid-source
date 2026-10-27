#pragma once
#include "NamBrowserLifetimeTests.h"
#include "Shared/UI/PopupInteraction.h"
#include "BetaWelcome.h"
#include "CalibrationAction.h"

namespace popupOnboardingTests
{
inline bool run(juce::String& report)
{
    using namespace juce;bool ok=true;int count=0;
    const auto check=[&](bool pass,const String& text){ok&=pass;++count;report+=String(pass?"PASS ":"FAIL ")+text+"\n";};
    namBrowserLifetimeTests::MouseHost board,parentSurface,childSurface;
    board.setSize(1000,700);parentSurface.setSize(1000,700);childSurface.setSize(1000,700);
    struct White:Component{void paint(Graphics& g)override{g.fillAll(Colours::white);}} white;
    board.addAndMakeVisible(white);white.setBounds(board.getLocalBounds());
    TextButton underlying("underlying");board.addAndMakeVisible(underlying);underlying.setBounds(10,10,120,40);
    int clicks=0;underlying.onClick=[&]{++clicks;};
    tonegrid::popup::Backdrop backdrop(board);
    tonegrid::popup::DismissibleDialog parent("Welcome",board,false),child("Settings",parent,false);
    parentSurface.addChildComponent(parent);childSurface.addChildComponent(child);
    parent.setBounds(200,100,500,450);child.setBounds(250,160,400,300);
    parent.enterModalState(false,nullptr,false);child.enterModalState(false,nullptr,false);
    for(int i=0;i<30;++i)backdrop.advance(1./60.);
    check(Component::getCurrentlyModalComponent()==&child&&parent.isVisible(),"Child dialog is modal above a retained Welcome");
    check(backdrop.isVisible()&&backdrop.getAlpha()==1&&backdrop.getBounds()==board.getLocalBounds(),"One backdrop covers the complete board with two dialogs open");
    const auto pixel=board.createComponentSnapshot(board.getLocalBounds()).getPixelAt(900,600);
    check(std::abs(int(pixel.getRed())-127)<=2&&pixel.getRed()==pixel.getGreen(),"Nested dialogs darken a white main interface by 50 percent, once");
    board.setSize(1100,740);check(backdrop.getBounds()==board.getLocalBounds(),"Backdrop follows board resizing");
    child.inputAttemptWhenModal();check(child.isVisible(),"Blocked keyboard focus attempts do not dismiss the popup");
    parentSurface.mouse().event({220,120},false);parentSurface.mouse().event({220,120},true);
    check(child.isVisible()&&parent.isVisible(),"Outside mouse-down defers dismissal until JUCE finishes routing the event");
    parentSurface.mouse().release();
    int parentResult=-1,childResult=-1;
    ModalComponentManager::getInstance()->attachCallback(&parent,ModalCallbackFunction::create([&](int result){parentResult=result;}));
    ModalComponentManager::getInstance()->attachCallback(&child,ModalCallbackFunction::create([&](int result){childResult=result;}));
    struct Poll:Timer{std::function<void()> action;void timerCallback()override{action();}} poll;
    int phase=0;
    poll.action=[&]
    {
        if(phase++==0)
        {
            check(!child.isVisible()&&parent.isVisible()&&Component::getCurrentlyModalComponent()==&parent,"Clicking Welcome closes only the Settings child");
            check(backdrop.isVisible()&&backdrop.getAlpha()==1,"Main shade remains while Welcome is still open");
            board.mouse().event({30,30},false);board.mouse().event({30,30},true);board.mouse().release();
            check(parent.isVisible(),"The parent also waits until the background click finishes routing");return;
        }
        check(!parent.isVisible()&&clicks==0,"Background click closes Welcome without activating the underlying button");
        for(int i=0;i<30;++i)backdrop.advance(1./60.);
        check(!backdrop.isVisible()&&backdrop.getAlpha()==0,"Last dialog dismissal fades the shade completely away");
        check(parentResult==0&&childResult==0,"Outside dismissal takes the cancellation path for both dialogs");
        poll.stopTimer();MessageManager::getInstance()->stopDispatchLoop();
    };
    poll.startTimer(30);MessageManager::getInstance()->runDispatchLoop();
    tonegrid::calibration::Action action;board.addAndMakeVisible(action);action.setBounds(200,10,128,32);
    for(int i=0;i<30;++i)action.advance(false,1./60.,0);
    check(action.getAlpha()==1&&action.needsAttention(),"Unfinished calibration stays visible outside the audio region");
    const float low=action.getProperties()["calibration-pulse"];action.advance(false,1./60.,.75);
    check(float(action.getProperties()["calibration-pulse"])>low+.5f,"The larger orange status dot pulses while calibration needs attention");
    action.setNeedsAttention(false);action.advance(false,1./60.,0);
    check(action.getAlpha()>0&&action.getAlpha()<1,"Completed calibration begins a gradual fade on mouse-out");
    for(int i=0;i<30;++i)action.advance(false,1./60.,0);
    check(action.getAlpha()==0,"Completed calibration hides completely after mouse-out");
    action.grabKeyboardFocus();for(int i=0;i<30;++i)action.advance(false,1./60.,0);
    check(action.getAlpha()==0,"Mouse-retained focus does not pin a completed calibration button");
    for(int i=0;i<30;++i)action.advance(true,1./60.,0);
    check(action.getAlpha()==1,"Entering the audio region reveals completed calibration");
    action.setNeedsAttention(true);for(int i=0;i<30;++i)action.advance(false,1./60.,0);
    check(action.getAlpha()==1,"A calibration requiring verification becomes permanently visible again");
    tonegrid::welcome::View view;bool settings=false,calibrate=false,suppress=true;
    view.onSettings=[&]{settings=true;};view.onCalibrate=[&]{calibrate=true;};view.onDismiss=[&](bool value){suppress=value;};
    view.settings.onClick();view.calibrate.onClick();view.close.onClick();
    check(settings&&calibrate&&!suppress,"Welcome exposes both setup actions and ordinary Close does not suppress future welcomes");
    for(int width:{380,640,900})
    {
        view.setSize(width,view.popupContentHeight(width));
        check(view.getLocalBounds().contains(view.settings.getBounds())&&view.getLocalBounds().contains(view.calibrate.getBounds())&&view.getLocalBounds().contains(view.close.getBounds())&&view.getLocalBounds().contains(view.never.getBounds()),"Welcome actions stay reachable at width "+String(width));
        check(view.settings.getBottom()<view.calibrate.getY()&&view.calibrate.getBottom()<view.close.getY(),"Welcome setup actions follow their sections at width "+String(width));
    }
    report+=String(count)+" popup and onboarding checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
