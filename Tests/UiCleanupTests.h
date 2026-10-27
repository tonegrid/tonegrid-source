#pragma once
#include "Modules/Play/UI/PedalCard.h"
#include "AmpChassis.h"
namespace uiCleanupTests
{
inline bool run(const juce::File& models,juce::String& report)
{
    using namespace juce;bool ok=true;int checks=0;auto check=[&](bool value,const String& message){++checks;ok&=value;report+=String(value?"PASS ":"FAIL ")+message+"\n";};
    for(int width:{1332,1632,2212})
    {
        juce::Rectangle<int> row(14+tonegrid::ui::logoWidth+tonegrid::ui::logoGap,14,width-tonegrid::ui::logoWidth-tonegrid::ui::logoGap,tonegrid::ui::headerHeight);const auto metrics=tonegrid::ui::toolbarMetrics(row.getWidth());auto groups=tonegrid::ui::toolbarSections(row);bool fits=true;
        for(size_t i=0;i<groups.size();++i){fits&=row.contains(groups[i]);if(i)fits&=groups[i].getX()-groups[i-1].getRight()>=metrics.sectionSpacing;}
        check(fits&&groups[0].getWidth()==metrics.preset+metrics.save+metrics.learn+2*metrics.controlGap+2*metrics.padding&&groups[5].getWidth()==metrics.midi+metrics.refresh+metrics.midiGap+2*metrics.padding&&groups[4].getWidth()==metrics.calibration+metrics.audio+32+2*metrics.controlGap+2*metrics.padding&&groups[3].getWidth()==112,"Toolbar groups including Pitch remain compact, non-overlapping and within window width "+String(width));
    }
    tonegrid::Look look;check(look.findColour(ComboBox::outlineColourId)==tonegrid::ui::outline(),"Shared combo outlines use neutral graphite instead of structural cyan");
    board::Processor processor;const auto loaded=processor.load(models);check(loaded.wasOk(),"UI cleanup tests load unchanged model catalog: "+loaded.getErrorMessage());if(loaded.failed())return false;
    for(int visual=0;visual<13;++visual)
    {
        const int id=PedalCard::engineFor(visual);const bool pilot=true;PedalCard card(visual,processor,false);card.setSize(224,288);auto* title=dynamic_cast<Label*>(card.findChildWithID("pedal-title"));auto* preset=dynamic_cast<ComboBox*>(card.findChildWithID("pedal-preset"));auto* midi=dynamic_cast<tonegrid::LearnButton*>(card.findChildWithID("pedal-midi"));auto* power=dynamic_cast<Button*>(card.findChildWithID("power"));
        bool lean=true;for(auto* c:card.getChildren())if(auto* label=dynamic_cast<Label*>(c))lean&=label->getText()!="NAM CAPTURE"&&label->getText()!="ALGORITHMIC"&&label->getText()!="Unassigned";
        check(title&&preset&&midi&&power&&lean&&card.findChildWithID("capture-info")==nullptr,"Pedal "+String(id)+" keeps controls/title and removes permanent technical chrome");if(!title||!preset||!midi||!power)continue;
        check(preset->getWidth()<=112&&midi->getWidth()<=104&&power->getY()>midi->getBottom()&&power->getWidth()==power->getHeight()&&card.getLocalBounds().contains(power->getBounds()),"Pedal "+String(id)+" has compact sub-controls above a usable centered round footswitch");
        const auto original=processor.get(id);const auto info=title->getTooltip();check(info.contains(board::isNam(id)?"Type: NAM Capture":"Type: Algorithmic")&&(!board::isNam(id)||processor.modelFile(id,original.preset).isEmpty()||info.contains(processor.modelFile(id,original.preset))),"Pedal "+String(id)+" title tooltip uses current type and real capture filename");
        power->onClick();check(processor.get(id).enabled==(processor.available(id,original.preset)?!original.enabled:false)&&power->getToggleState()==processor.get(id).enabled,"Pedal "+String(id)+" chrome click performs existing bypass action once");
        processor.set(id,original);card.sync();
        if(preset->getNumItems()>1){const int selected=preset->getItemId(1);preset->setSelectedId(selected,sendNotificationSync);check(processor.get(id).preset==selected-1&&title->getTooltip().contains(processor.label(id,selected-1)),"Pedal "+String(id)+" compact preset retains selection and updates current metadata");processor.set(id,original);card.sync();}
        bool learn=false,clear=false;card.onLearn=[&]{learn=true;};card.onClear=[&]{clear=true;};card.midi({},false,true);check(midi->getButtonText()==(pilot?"MIDI":"MIDI Learn"),"Pedal "+String(id)+" unassigned MIDI area shows only Learn");midi->onClick();midi->onClear();check(learn&&clear,"Pedal "+String(id)+" Learn and existing right-click Clear callbacks retained");
        card.midi({1,5,1},false,true);check(midi->getButtonText()==(pilot?"PC 1 / Ch 5":"PC 1 / channel 5")&&midi->getTooltip().contains("learn a new assignment"),"Pedal "+String(id)+" MIDI assignment stays visible inside compact control");card.midi({1,5,1},true,true);check(midi->getButtonText()==(pilot?"Cancel":"Cancel Learn")&&midi->getTooltip().contains("controller"),"Pedal "+String(id)+" learn/cancel state remains accessible");
        card.midi({},false,true);card.setSize(188,240);check(card.getLocalBounds().contains(power->getBounds())&&power->getWidth()>=(pilot?29:30)&&preset->getHeight()>=(pilot?16:23)&&midi->getHeight()>=(pilot?15:20),"Pedal "+String(id)+" compact controls remain usable at minimum card size");
    }
    report+=String(checks)+" UI cleanup layout/tooltip/pedal interaction checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
