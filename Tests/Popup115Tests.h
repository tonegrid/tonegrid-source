#pragma once
#include "Shared/UI/PopupWindow.h"
#include "Shared/UI/Library/NamLibraryBrowser.h"

namespace popup115Tests
{
inline bool run(juce::String& report)
{
    using namespace juce;
    bool ok=true;int checks=0;
    auto check=[&](bool pass,const String& message){++checks;ok&=pass;report+=String(pass?"PASS ":"FAIL ")+"Popup 1.1.15 "+message+"\n";};
    struct Content final : tonegrid::popup::View
    {
        explicit Content(int& count):destroyed(count){setSize(700,440);}
        ~Content() override{++destroyed;}
        int& destroyed;
    };
    int destroyed=0;
    Component main;LookAndFeel_V4 mainLook;main.setLookAndFeel(&mainLook);
    const auto original=main.findColour(ComboBox::backgroundColourId);
    {
        Content persistent(destroyed);
        for(int repeat=0;repeat<3;++repeat)
        {
            {tonegrid::popup::Frame frame(persistent,false,"Persistent",0);check(persistent.getParentComponent()!=nullptr,"persistent view attaches on opening "+String(repeat));}
            check(!persistent.getParentComponent()&&destroyed==0,"closing releases persistent view without destroying it "+String(repeat));
        }
        tonegrid::popup::Frame small(persistent,false,"Small display",0);small.setSize(600,400);
        auto* viewport=persistent.findParentComponentOfClass<Viewport>();
        check(viewport&&persistent.getHeight()==440&&viewport->getMaximumVisibleHeight()<persistent.getHeight()&&viewport->getVerticalScrollBar().isVisible(),"small display preserves fixed controls and provides scrolling");
        if(viewport)viewport->setViewPosition(0,440);
        check(viewport&&viewport->getViewPositionY()>0,"last controls remain reachable on a small display");
        for(const auto size:std::array<juce::Point<int>,2>{{{600,400},{784,588}}})
        {
            small.setSize(size.x,size.y);
            Image pixels(Image::ARGB,size.x,size.y,true,SoftwareImageType{});
            {Graphics graphics(pixels);small.paintEntireComponent(graphics,true);}
            check(pixels.getPixelAt(30,190).getAlpha()==255&&pixels.getPixelAt(size.x/2,100).getAlpha()==255,
                  "panel interior stays fully opaque at "+String(size.x)+" x "+String(size.y));
            check(pixels.getPixelAt(0,0).getAlpha()==0&&pixels.getPixelAt(size.x/2,12).getAlpha()>0&&pixels.getPixelAt(size.x/2,12).getAlpha()<255,
                  "only outside margins and soft shadow are translucent at "+String(size.x)+" x "+String(size.y));
        }
    }
    check(destroyed==1,"persistent view is destroyed only by its owner");
    {tonegrid::popup::Frame owned(*new Content(destroyed),true,"Owned",0);}
    check(destroyed==2,"short-lived owned content is destroyed exactly once");
    {
        tonegrid::NamLibraryBrowser library;library.setSize(1240,640);
        for(int repeat=0;repeat<3;++repeat)
        {
            {
                tonegrid::popup::Frame frame(library,false,"Local NAM Library",360);
                auto* title=dynamic_cast<Label*>(frame.findChildWithID("popup-title"));
                auto* mark=frame.findChildWithID("popup-brand-mark");
                check(title&&mark&&title->getText()=="LOCAL NAM LIBRARY"&&title->getFont().getHeight()==22.f
                      &&mark->getWidth()==34&&title->getX()>mark->getRight()
                      &&!title->getBounds().intersects(library.captures.getBounds()),
                      "large single title, pictogram and Tone3000 action fit the header "+String(repeat));
                check(library.popupHeaderAction()->getParentComponent()==&frame&&library.list.getHeight()>=400&&library.refresh.getBottom()==library.getHeight()
                      &&library.refresh.getY()==library.openFolder.getY()&&!library.list.getBounds().intersects(library.refresh.getBounds()),
                      "global search/CTA share header and actions stay at bottom "+String(repeat));
                const int fullHeight=library.list.getHeight();library.setImportStatus("Imported and loaded capture");
                check(library.list.getHeight()==fullHeight,"import feedback uses footer without reducing list height "+String(repeat));
                library.setImportStatus({});
            }
            check(!library.getParentComponent()&&library.popupHeaderAction()->getParentComponent()==&library,
                  "body-owned CTA survives header destruction and can reopen "+String(repeat));
        }
    }
    check(main.findColour(ComboBox::backgroundColourId)==original && &main.getLookAndFeel()==&mainLook,"popup themes leave the main window look intact");
    main.setLookAndFeel(nullptr);
    report+=String(checks)+" popup lifetime and small-display checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";
    return ok;
}
}
