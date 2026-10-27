#pragma once
#include "Modules/Play/UI/PedalCard.h"

namespace captureSelector120Tests
{
inline bool run(const juce::File& models,const juce::File& evidence,juce::String& report)
{
    using namespace juce;bool ok=true;int checks=0;auto check=[&](bool pass,const String& name){++checks;ok&=pass;report+=String(pass?"PASS ":"FAIL ")+"CaptureSelector120 "+name+"\n";};
    board::Processor engine;check(engine.load(models).wasOk(),"production library loads");if(!ok)return false;evidence.createDirectory();
    Component surface;surface.setOpaque(true);surface.addToDesktop(ComponentPeer::windowIsTemporary);surface.setVisible(false);
    std::vector<std::unique_ptr<PedalCard>> cards;
    for(int visual=0;visual<board::count;++visual){auto card=std::make_unique<PedalCard>(visual,engine);surface.addAndMakeVisible(*card);cards.push_back(std::move(card));}
    for(int visual=0;visual<board::count;++visual)
    {
        auto& card=*cards[size_t(visual)];const int id=PedalCard::engineFor(visual);
        auto verify=[&](const String& phase)
        {
            auto* combo=dynamic_cast<ComboBox*>(card.findChildWithID("pedal-preset"));
            PopupMenu::MenuItemIterator it(*combo->getRootMenu());
            const bool first=it.next();const auto& header=it.getItem();
            check(first&&header.text==engine.captureSection(id,0)&&(board::isNam(id)?header.isEnabled&&header.itemID==PedalCard::creatorFirstItem&&dynamic_cast<CreatorCaptureHeader*>(header.customComponent.get())!=nullptr:header.isSectionHeader&&header.itemID==0),
                phase+" begins with a clickable creator profile or a ToneGrid algorithm heading for pedal "+String(id));
            int headings=0;bool waza=false;
            for(PopupMenu::MenuItemIterator row(*combo->getRootMenu());row.next();){headings+=row.getItem().isSectionHeader||dynamic_cast<CreatorCaptureHeader*>(row.getItem().customComponent.get())!=nullptr;waza|=row.getItem().text.containsIgnoreCase("waza");}
            check(headings==1&&!waza,phase+" creator group is present and withdrawn Waza/DS-1 groups are absent for pedal "+String(id));
        };
        verify("Initial menu");
        if(board::isNam(id)){card.setPersonalCaptures({});verify("Refreshed menu");}
    }
    for(int width:{1360,1660,1920})for(float scale:{1.f,1.25f,1.5f})
    {
        surface.setSize(width,760);
        const int cardWidth=width/7,cardHeight=350;
        for(int visual=0;visual<board::count;++visual)
        {
            auto& card=*cards[size_t(visual)];card.setBounds((visual%7)*cardWidth,(visual/7)*cardHeight,cardWidth,cardHeight);
            const int id=PedalCard::engineFor(visual);auto* combo=dynamic_cast<ComboBox*>(card.findChildWithID("pedal-preset"));
            const int n=board::presetCount(id)-1;combo->setSelectedId(n+1,sendNotificationSync);
            Label* label=nullptr;for(int c=0;c<combo->getNumChildComponents();++c)if(auto* l=dynamic_cast<Label*>(combo->getChildComponent(c)))label=l;
            const auto full=engine.label(id,n);auto* handler=combo->getAccessibilityHandler();auto* value=handler?handler->getValueInterface():nullptr;
            check(label&&label->getText()==full&&combo->getText()==full&&combo->getTooltip().contains(full)&&value&&value->getCurrentValueAsString()==full,"complete name retained in value, tooltip, accessibility at "+String(width)+" scale "+String(scale)+" pedal "+String(id));
            if(!label)continue;
            auto f=card.getLookAndFeel().getComboBoxFont(*combo);auto shown=tonegrid::captureui::curtailed(full,f,float(label->getWidth()));
            check(f.getHeight()==12.f&&label->getMinimumHorizontalScale()==1.f&&label->getRight()<=combo->getWidth()-20&&label->getHeight()>=f.getHeight()&&GlyphArrangement::getStringWidth(f,shown)<=label->getWidth()+.01f&&!shown.containsAnyOf("\r\n")&&(shown==full||shown.endsWithChar(0x2026)),"single fixed-font line fits text/arrow bounds at "+String(width)+" scale "+String(scale)+" pedal "+String(id));
        }
        MemoryOutputStream png;PNGImageFormat().writeImageToStream(surface.createComponentSnapshot(surface.getLocalBounds(),true,scale),png);evidence.getChildFile("selectors-"+String(width)+"-"+String(roundToInt(scale*100))+".png").replaceWithData(png.getData(),png.getDataSize());
    }
    // Render the real JUCE menu component inside the evidence surface. Full names
    // and group headings use the production LookAndFeel and measured menu width.
    for(const int visual:{7,9})
    {
        auto& card=*cards[size_t(visual)];auto* combo=dynamic_cast<ComboBox*>(card.findChildWithID("pedal-preset"));Label* label=nullptr;
        for(int c=0;c<combo->getNumChildComponents();++c)if(auto* l=dynamic_cast<Label*>(combo->getChildComponent(c)))label=l;
        if(!label)continue;
        auto options=card.getLookAndFeel().getOptionsForComboBoxPopupMenu(*combo,*label).withParentComponent(&surface).withTargetScreenArea(surface.localAreaToGlobal(juce::Rectangle<int>(20,20,combo->getWidth(),combo->getHeight()))).withItemThatMustBeVisible(1);
        combo->getRootMenu()->setLookAndFeel(&card.getLookAndFeel());combo->getRootMenu()->showMenuAsync(options,[](int){});
        Component* menu=nullptr;for(int c=0;c<surface.getNumChildComponents();++c){auto* child=surface.getChildComponent(c);if(child!=&card&&child->getName().containsIgnoreCase("menu"))menu=child;}
        if(!menu)for(int c=0;c<surface.getNumChildComponents();++c){auto* child=surface.getChildComponent(c);if(dynamic_cast<PedalCard*>(child)==nullptr)menu=child;}
        check(menu&&menu->getWidth()>=options.getMinimumWidth(),"real open menu uses full-name width for pedal "+String(PedalCard::engineFor(visual)));
        if(menu)for(float scale:{1.f,1.25f,1.5f}){MemoryOutputStream png;PNGImageFormat().writeImageToStream(menu->createComponentSnapshot(menu->getLocalBounds(),true,scale),png);evidence.getChildFile("menu-"+String(PedalCard::engineFor(visual))+"-"+String(roundToInt(scale*100))+".png").replaceWithData(png.getData(),png.getDataSize());}
        PopupMenu::dismissAllActiveMenus();combo->getRootMenu()->setLookAndFeel(nullptr);
    }
    report+=String(checks)+" capture selector 1.1.20 checks; "+String(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
