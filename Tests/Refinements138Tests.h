#pragma once
#include "Modules/Play/UI/PedalCard.h"
#include "Content/Presets/RigCodec.h"
#include "Content/Presets/ToneBank.h"
namespace refinements138Tests
{
inline bool run(const juce::File& directory,juce::String& report)
{
    using namespace juce;bool ok=true;int checks=0;
    auto check=[&](bool pass,const String& text){++checks;ok&=pass;report+=String(pass?"PASS ":"FAIL ")+text+"\n";};
    board::Processor processor;const auto loaded=processor.load(directory);check(loaded.wasOk(),"new factory catalogue loads");if(loaded.failed())return false;
    check(board::presetCount(10)==21&&board::recommendedPreset(10)==10,"Blues OD offers all 21 supplied captures and the noon recommendation");
    for(int n=0;n<21;++n)
    {const auto info=processor.calibrationInfo(10,n);check(info.input==10.5&&info.output==10.5&&processor.details(10,n).contains("MaestrodiMusica"),"supplied BD-2 references and attribution retained "+String(n));}
    for(int visual=0;visual<13;++visual)
    {
        const int pedal=PedalCard::engineFor(visual);if(!board::isNam(pedal))continue;
        PedalCard card(visual,processor);auto* combo=dynamic_cast<ComboBox*>(card.findChildWithID("pedal-preset"));
        String opened;int calls=0;card.onOpenCreator=[&](const String& url){opened=url;++calls;return true;};
        const auto before=processor.get(pedal);const auto selected=combo->getSelectedId();int links=0;
        for(PopupMenu::MenuItemIterator iterator(*combo->getRootMenu());iterator.next();)
        {
            const auto& item=iterator.getItem();if(item.itemID<PedalCard::creatorFirstItem)continue;++links;
            check(item.isEnabled&&!item.isSectionHeader&&dynamic_cast<CreatorCaptureHeader*>(item.customComponent.get()),"creator header is an accessible clickable menu action");
            combo->setSelectedId(item.itemID,sendNotificationSync);
            check(CreatorCaptureHeader::validProfile(opened)&&combo->getSelectedId()==selected&&processor.get(pedal).preset==before.preset&&processor.get(pedal).enabled==before.enabled,"opening creator profile retains selected capture and bypass state");
        }
        check(links>0&&calls==links,"every factory creator header opens exactly once");
    }
    for(const auto& bad:StringArray{"http://www.tone3000.com/person","https://evil.example/person","https://www.tone3000.com/a/b","file:///capture","https://www.tone3000.com/person?url=file"})check(!CreatorCaptureHeader::validProfile(bad),"reject non-profile creator URL");
    const board::RigIdentityLookup lookup=[&](int p,int n){return processor.identity(p,n);};auto state=board::migrations::initialState();state.pedals[10].enabled=true;state.pedals[10].input=2;state.pedals[10].tone=-1;state.pedals[10].output=-3;
    state.sourceType=amp::SourceType::Nam;state.namAmp.model={"Factory test.nam",String::repeatedString("b",64),100};state.outputDsp=tonegrid::output::freshSessionSettings();
    const auto make=[&](){return board::encode(state,{},lookup);};board::State decoded;
    const auto baseline=board::decode(make(),{},lookup,decoded);check(baseline.wasOk(),"valid isolated NAM rig fixture: "+baseline.getErrorMessage());
    auto rig=make();auto* blues=rig.getChildByAttribute("id","10");blues->setAttribute("preset",6);blues->setAttribute("model","d2ca82812d53d2cd47e768c0a230f0e653d62217bf10f0f36dd0192f180d52f7");
    check(board::decode(rig,{},lookup,decoded).wasOk()&&decoded.pedals[10].preset==10&&decoded.pedals[10].enabled&&decoded.pedals[10].input==2&&decoded.pedals[10].tone==-1&&decoded.pedals[10].output==-3&&decoded.missingCaptureDiagnostics.isEmpty(),"former noon factory identity restores with matching controls and no missing-capture alert");
    for(int slot:{14,15})
    {
        rig=make();rig.setAttribute("version",15);blues=rig.getChildByAttribute("id","10");blues->setAttribute("preset",slot);blues->setAttribute("model",String::repeatedString("a",64));
        check(board::decode(rig,{},lookup,decoded).wasOk()&&decoded.pedals[10].preset==21&&decoded.customPedalHashes[10]==String::repeatedString("a",64),"former personal slot survives the expanded factory catalogue");
        state.pedals[10].preset=slot;rig=make();check(board::decode(rig,{},lookup,decoded).wasOk()&&decoded.pedals[10].preset==slot&&decoded.customPedalHashes[10].isEmpty(),"new factory indices 14 and 15 are never mistaken for personal captures");
    }
    const auto& eq=tonegrid::assets::pedal137(11);bool aligned=true;
    for(const auto& fader:eq.faders)
    {
        const auto expected=fader.getCentreX();double sum=0,mass=0;
        for(int x=roundToInt(expected)-26;x<=roundToInt(expected)+26;++x){const auto c=eq.face.getPixelAt(x,560);const auto weight=jmax(0.,180.-(c.getRed()+c.getGreen()+c.getBlue())/3.);sum+=x*weight;mass+=weight;}
        aligned&=mass>0&&std::abs(sum/mass-expected)<.15;
    }
    check(aligned,"all seven EQ control axes match the unchanged bitmap slots");
    report+=String(checks)+" 1.1.38 refinement checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
