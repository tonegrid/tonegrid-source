#pragma once
#include <iostream>
#include <cstdlib>
#ifndef TONEGRID_BROWSER_TEST_HEADER
#define TONEGRID_BROWSER_TEST_HEADER "Shared/UI/Library/NamLibraryBrowser.h"
#endif
#include TONEGRID_BROWSER_TEST_HEADER

namespace namBrowserLifetimeTests
{
// A registered, windowless JUCE peer feeds the REAL MouseInputSource/ListBox
// event path without moving the user's cursor or depending on window occlusion.
// It supplies only OS window services; no ListBox/item behavior is mocked.
class MousePeer final:public juce::ComponentPeer
{
public:
    explicit MousePeer(juce::Component& component):ComponentPeer(component,0){}
    void* getNativeHandle() const override{return nullptr;}
    void setVisible(bool) override{}
    void setTitle(const juce::String&) override{}
    void setBounds(const juce::Rectangle<int>&,bool) override{}
    juce::Rectangle<int> getBounds() const override{return getComponentBounds();}
    juce::Point<float> localToGlobal(juce::Point<float> p) override{return p;}
    juce::Point<float> globalToLocal(juce::Point<float> p) override{return p;}
    void setMinimised(bool) override{}
    bool isMinimised() const override{return false;}
    bool isShowing() const override{return true;}
    void setFullScreen(bool) override{}
    bool isFullScreen() const override{return false;}
    void setIcon(const juce::Image&) override{}
    bool contains(juce::Point<int> p,bool) const override{return getComponentBounds().contains(p);}
    OptionalBorderSize getFrameSizeIfPresent() const override{return OptionalBorderSize(juce::BorderSize<int>{});}
    juce::BorderSize<int> getFrameSize() const override{return {};}
    bool setAlwaysOnTop(bool) override{return false;}
    void toFront(bool) override{}
    void toBehind(ComponentPeer*) override{}
    bool isFocused() const override{return focused;}
    void focusForTest(){focused=true;}
    void grabFocus() override{}
    void repaint(const juce::Rectangle<int>&) override{}
    void performAnyPendingRepaintsNow() override{}
    void setAlpha(float) override{}
    juce::StringArray getAvailableRenderingEngines() override{return {};}
    void textInputRequired(juce::Point<int>,juce::TextInputTarget&) override{}
    void event(juce::Point<float> p,bool held)
    {
        last=p;handleMouseEvent(juce::MouseInputSource::mouse,p,
            juce::ModifierKeys(held?juce::ModifierKeys::leftButtonModifier:0),0,0,++time);
    }
    void release(){event(last,false);}
    bool press(tonegrid::NamLibraryBrowser& browser,const juce::String& path,
               juce::Component::SafePointer<juce::Component>& row,bool continueClick=false)
    {
        if(!continueClick)time+=1000;
        if(!browser.reveal(path))return false;
        row=browser.rowComponent(path);if(!row)return false;
        last=getComponent().getLocalPoint(row.getComponent(),row->getLocalBounds().getCentre().toFloat());
        event(last,false);event(last,true);
        const auto mouse=juce::Desktop::getInstance().getMainMouseSource();
        const bool captured=mouse.isDragging()&&mouse.getComponentUnderMouse()==row.getComponent()&&browser.isUpdatePending();
        if(!captured){auto* under=mouse.getComponentUnderMouse();std::cout<<"Mouse capture failure: dragging="<<mouse.isDragging()<<" row="<<(under==row.getComponent())<<" pending="<<browser.isUpdatePending()<<" position="<<last.toString()<<" hit="<<(getComponent().getComponentAt(last)==row.getComponent())<<" under="<<(under?typeid(*under).name():"null")<<" rowbounds="<<row->getBounds().toString()<<" scale="<<getComponent().getDesktopScaleFactor()<<"\n";}
        return captured;
    }
private:
    juce::Rectangle<int> getComponentBounds() const{return const_cast<MousePeer*>(this)->getComponent().getLocalBounds();}
    bool focused=false;juce::int64 time=juce::Time::currentTimeMillis();juce::Point<float> last;
};

class MouseHost final:public juce::Component
{
public:
    MouseHost(){setSize(1240,640);addToDesktop(0);setVisible(true);}
    MousePeer& mouse(){return *static_cast<MousePeer*>(getPeer());}
private:
    juce::ComponentPeer* createNewPeer(int,void*) override{return new MousePeer(*this);}
};

inline bool run(juce::String& report,int iterations=1000)
{
    using namespace juce;bool ok=true;int checks=0;
    auto check=[&](bool pass,const String& name){++checks;ok&=pass;report+=String(pass?"PASS ":"FAIL ")+name+"\n";};
    const auto temp=File::getSpecialLocation(File::tempDirectory),root=temp.getChildFile("ToneGrid-list-lifetime-"+Uuid().toString());
    ScopeGuard cleanup{[&]{if(root.isAChildOf(temp))root.deleteRecursively();}};
    for(const auto& path:{"A.nam","Nested/B.nam","Nested/Deep/A.nam"})
    {auto file=root.getChildFile(path);file.getParentDirectory().createDirectory();file.replaceWithText(R"({"version":"0.7.0","architecture":"Linear","sample_rate":48000,"config":{"receptive_field":1,"bias":false},"weights":[1]})");}
    namamp::Library library(root);namamp::Catalog catalog;check(library.refresh(catalog).wasOk(),"Lifetime fixtures use only a fresh synthetic temporary library");
    MouseHost host;auto& peer=host.mouse();
    auto browser=std::make_unique<tonegrid::NamLibraryBrowser>();host.addAndMakeVisible(*browser);
    String active;int callbacks=0;
    auto update=[&]{browser->update(catalog,root,active);if(active.isNotEmpty())browser->reveal(active);};
    browser->onSelect=[&](const String& path){++callbacks;active=path;update();};update();
    bool allCaptured=true,allRetired=true,allSelected=true,allRendered=true;int completed=0;
    for(int i=0;i<iterations;++i)
    {
        const String path=i%2?"Nested/Deep/A.nam":"Nested/B.nam";
        Component::SafePointer<Component> row;
        if(!peer.press(*browser,path,row)){allCaptured=false;peer.release();break;}
        browser->handleUpdateNowIfNeeded();
        allRetired&=!row||row->getComponentID()=="nam-row:"+path;
        allSelected&=active==path&&callbacks==i+1&&browser->find(path)->isSelected();
        peer.release();if(i%25==0)allRendered&=browser->createComponentSnapshot(browser->getLocalBounds()).isValid();
        ++completed;
    }
    check(allCaptured&&completed==iterations,"All "+String(iterations)+" selections enter real JUCE mouse-held row capture");
    check(allRetired&&completed==iterations,"All "+String(iterations)+" rebuilds retain only value-owned rows for the same capture path");
    check(allSelected&&completed==iterations,"All "+String(iterations)+" callbacks preserve relative identity and selected state");
    check(allRendered&&completed==iterations,"Mouse release after every held selection and periodic repaint remain safe");
    browser->reveal("Nested/B.nam");auto* favoriteRow=browser->rowComponent("Nested/B.nam");auto* star=favoriteRow?favoriteRow->findChildWithID("nam-library-favorite"):nullptr;
    const int beforeStar=callbacks;
    if(star){const auto point=host.getLocalPoint(star,star->getLocalBounds().getCentre().toFloat());peer.event(point,false);peer.event(point,true);peer.release();browser->handleUpdateNowIfNeeded();}
    check(star&&browser->isFavorite("Nested/B.nam")&&callbacks==beforeStar,"Real star mouse click toggles favorite without row activation/loading");
    int closes=0;browser->onClose=[&]{++closes;};Component::SafePointer<Component> firstClick,secondClick;
    bool doubleCaptured=peer.press(*browser,"A.nam",firstClick);browser->handleUpdateNowIfNeeded();peer.release();
    doubleCaptured&=peer.press(*browser,"A.nam",secondClick,true);browser->handleUpdateNowIfNeeded();peer.release();browser->handleUpdateNowIfNeeded();
    check(doubleCaptured&&firstClick&&firstClick==secondClick&&closes==1,"Real double click survives first model commit and calls close once");
    browser->onClose={};
    Component::SafePointer<Component> row;
    bool captured=peer.press(*browser,"A.nam",row);const auto before=callbacks;update();
    check(captured&&(!row||row->getComponentID()=="nam-row:A.nam")&&!browser->isUpdatePending(),"Refresh during mouse capture keeps rows safe and cancels queued selection");
    peer.release();browser->handleUpdateNowIfNeeded();check(callbacks==before,"Cancelled selection never runs after Refresh");
    browser->find("A.nam")->activate();browser->find("Nested/B.nam")->activate();browser->handleUpdateNowIfNeeded();
    check(callbacks==before+1&&active=="Nested/B.nam","Rapid pending selections coalesce to latest valid relative identity");
    browser->onSelect=[&](const String&){++callbacks;update();};
    captured=peer.press(*browser,"A.nam",row);browser->handleUpdateNowIfNeeded();
    check(captured&&(!row||row->getComponentID()=="nam-row:A.nam")&&active=="Nested/B.nam","Failed-load UI resynchronization keeps captured row safe and preserves previous active model");peer.release();
    captured=peer.press(*browser,"A.nam",row);const auto beforeClose=callbacks;browser.reset();
    check(captured&&!row,"Closing browser while mouse held deletes captured row before list destruction");
    peer.release();check(callbacks==beforeClose,"Browser destruction cancels pending selection callback");
    report+=String(checks)+" NAM browser lifetime checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
