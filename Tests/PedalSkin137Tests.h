#pragma once
#include "Modules/Play/UI/PedalCard.h"
#include "Modules/Play/UI/PedalScene.h"
#include "Modules/Play/UI/PedalMidiLearnView.h"

namespace pedalSkin137Tests
{
inline bool same(const board::Setting& a,const board::Setting& b)
{return a.preset==b.preset&&a.enabled==b.enabled&&a.input==b.input&&a.mix==b.mix&&a.output==b.output&&a.ratio==b.ratio&&a.tone==b.tone&&a.wahSensitivity==b.wahSensitivity&&a.wahRange==b.wahRange&&a.wahDecay==b.wahDecay&&a.eqBands==b.eqBands;}
inline bool run(const juce::File& models,const juce::File& output,juce::String& report)
{
    using namespace juce;bool ok=true;int checks=0;
    const auto check=[&](bool pass,const String& message){++checks;ok&=pass;report+=String(pass?"PASS ":"FAIL ")+message+"\n";};
    output.createDirectory();
    auto save=[&](const String& name,const Image& image){auto stream=output.getChildFile(name).createOutputStream();if(stream){stream->setPosition(0);stream->truncate();return PNGImageFormat().writeImageToStream(image,*stream);}return false;};
    board::Processor engine;auto loaded=engine.load(models);check(loaded.wasOk(),"unchanged pedal audio catalogue loads");if(loaded.failed())return false;
    for(int v=0;v<13;++v)
    {
        const auto& skin=tonegrid::assets::pedal137(v);const auto id=PedalCard::engineFor(v);const auto before=engine.get(id);
        check(skin.valid&&skin.bakedKnobs&&skin.face.isValid(),"supplied skin is available for pedal "+String(v+1));
        check(skin.registeredBox&&skin.jackAxis==710&&skin.face.getPixelAt(16,710).getAlpha()>128&&skin.face.getPixelAt(skin.face.getWidth()-17,710).getAlpha()>128,"both metal jack tips reach the shared contact axis");
        if(v!=11)
        {
            bool clean=true,onlyOneMarker=true;
            tonegrid::assets::Look markerLook(skin);
            for(int k=0;k<3;++k)
            {
                const auto knob=skin.knobs[size_t(k)];const auto centre=knob.getCentre();const auto radius=knob.getWidth()*.5f;
                for(int y=roundToInt(centre.y-radius*.70f);y<roundToInt(centre.y-radius*.44f);++y)
                    for(int x=roundToInt(centre.x)-2;x<=roundToInt(centre.x)+2;++x)
                    {const auto pixel=skin.face.getPixelAt(x,y);clean&=juce::jmin(pixel.getRed(),pixel.getGreen(),pixel.getBlue())<150;}
                Slider knobSlider;knobSlider.setSize(100,100);knobSlider.getProperties().set("skin-knob-index",k);
                for(float position:{0.f,.5f,1.f})
                {
                    Image overlay(Image::ARGB,100,100,true);{Graphics g(overlay);markerLook.drawRotarySlider(g,0,0,100,100,position,0,0,knobSlider);}
                    int painted=0;for(int y=0;y<100;++y)for(int x=0;x<100;++x)painted+=overlay.getPixelAt(x,y).getAlpha()>0;
                    const auto angle=(position-.5f)*MathConstants<float>::pi*1.5f;
                    const int mx=roundToInt(50+std::sin(angle)*32),my=roundToInt(50-std::cos(angle)*32);
                    onlyOneMarker&=painted>50&&painted<250&&overlay.getPixelAt(mx,my).getAlpha()>200;
                    if(position!=.5f)onlyOneMarker&=overlay.getPixelAt(50,18).getAlpha()==0;
                }
            }
            check(clean,"all three source knob faces have no baked white indicator");
            check(onlyOneMarker,"JUCE draws only one indicator at minimum, centre and maximum; body pixels stay stationary");
        }
        PedalCard card(v,engine);card.setSize(330,470);card.midi({},false,true);
        bool titleClear=true;for(auto* c:card.getChildren())if(auto* label=dynamic_cast<Label*>(c))titleClear&=label->getTooltip().isEmpty();
        check(titleClear,"pedal title no longer exposes the large technical tooltip");
        auto* preset=dynamic_cast<ComboBox*>(card.findChildWithID("pedal-preset"));auto* power=dynamic_cast<Button*>(card.findChildWithID("power"));auto* hidden=card.findChildWithID("pedal-midi");
        check(preset&&power&&(!hidden||!hidden->isVisible()),"no permanent MIDI button occupies the pedal face");
        if(!preset||!power)continue;
        check(preset->getItemId(preset->getNumItems()-1)==PedalCard::midiLearnItem&&preset->getItemText(preset->getNumItems()-1)=="LEARN MIDI","LEARN MIDI is the final dropdown action");
        int calls=0;card.onLearn=[&]{++calls;};const auto selected=preset->getSelectedId();
        preset->setSelectedId(PedalCard::midiLearnItem,sendNotificationSync);
        check(calls==1&&preset->getSelectedId()==selected&&same(engine.get(id),before),"opening MIDI learn preserves capture selection and every pedal parameter");
        if(board::isNam(id))
        {
            card.setPersonalCaptures({{String::repeatedString("a",64),"Personal capture"}});
            check(preset->getItemId(preset->getNumItems()-1)==PedalCard::midiLearnItem&&preset->indexOfItemId(900001)>=0&&preset->indexOfItemId(900002)>=0,"personal capture refresh retains import actions and final MIDI action");
        }
        check(card.getLocalBounds().contains(power->getBounds())&&card.getLocalBounds().contains(preset->getBounds()),"switch and selector fit inside the pedal");
        if(v==11)
        {
            int faders=0;for(auto* c:card.getChildren())if(c->getComponentID().startsWith("eq-band-")&&c->isVisible())++faders;
            check(faders==7&&skin.face.getPixelAt(0,0).getAlpha()==0,"EQ retains seven faders and no opaque white exterior");
        }
        const auto off=card.createComponentSnapshot(card.getLocalBounds());save("pedal-"+String(v+1).paddedLeft('0',2)+".png",off);
        auto original=engine.get(id);power->onClick();check(engine.get(id).enabled==(engine.available(id,original.preset)?!original.enabled:false),"footswitch retains the existing bypass action");engine.set(id,original);card.sync();
    }
    Image flat(Image::ARGB,700,400,true);{Graphics g(flat);tonegrid::assets::paintPedalScene(g,flat.getBounds());}
    bool uniform=true;for(int y=0;y<400;y+=7)for(int x=0;x<700;x+=7)uniform&=flat.getPixelAt(x,y)==Colour(0xff0c151a);
    check(uniform,"central background is one flat colour without contact-shadow overlays");
    for(const auto size:std::array<Point<int>,3>{{{1100,360},{1380,560},{1900,720}}})
    {
        const juce::Rectangle<int> area(0,0,size.x,size.y);const auto positions=tonegrid::assets::boardBounds(area);bool fit=true;
        for(size_t i=0;i<positions.size();++i){fit&=area.contains(positions[i]);for(size_t j=0;j<i;++j)fit&=!positions[i].intersects(positions[j]);}
        check(fit,"all thirteen pedal hit areas remain separate at "+String(size.x)+"x"+String(size.y));
        bool connected=true;for(int row=0;row<2;++row){const int first=row?7:0,last=row?12:6;for(int v=first;v<last;++v){const auto a=positions[size_t(v)],b=positions[size_t(v+1)];const auto& sa=tonegrid::assets::pedal137(v);const auto& sb=tonegrid::assets::pedal137(v+1);const auto ay=sa.logicalToLocal({16,sa.jackAxis,1,1},a).getY(),by=sb.logicalToLocal({16,sb.jackAxis,1,1},b).getY();connected&=a.getRight()==b.getX()&&std::abs(ay-by)<.01f;}}
        check(connected,"adjacent jack tips touch on one horizontal axis at "+String(size.x)+"x"+String(size.y));
        Component panel;panel.setSize(size.x,size.y);
        std::array<std::unique_ptr<PedalCard>,13> cards;
        for(int v=0;v<13;++v){cards[size_t(v)]=std::make_unique<PedalCard>(v,engine);panel.addAndMakeVisible(*cards[size_t(v)]);cards[size_t(v)]->setBounds(positions[size_t(v)]);}
        Image image(Image::ARGB,size.x,size.y,true);{Graphics g(image);tonegrid::assets::paintPedalScene(g,area);g.drawImageAt(panel.createComponentSnapshot(area),0,0);}
        save("pedalboard-"+String(size.x)+".png",image);
    }
    {
        const auto root=output.getChildFile("external-skin-fixture");const tonegrid::assets::SkinFolders137 folders{root.getChildFile("user"),root.getChildFile("bundled")};folders.user.createDirectory();folders.bundled.createDirectory();
        const auto write=[&](const File& file,const Image& image){auto stream=file.createOutputStream();if(!stream)return false;stream->setPosition(0);stream->truncate();return PNGImageFormat().writeImageToStream(image,*stream);};
        Image embedded(Image::ARGB,8,8,true),shipped(Image::ARGB,8,8,true),edited(Image::ARGB,8,8,true);embedded.clear(embedded.getBounds(),Colours::red);shipped.clear(shipped.getBounds(),Colours::green);edited.clear(edited.getBounds(),Colours::blue);
        const auto user=folders.user.getChildFile("face01.png"),bundle=folders.bundled.getChildFile("face01.png");
        user.deleteFile();bundle.deleteFile();
        check(tonegrid::assets::loadSkinPng137(folders,"face01.png",{8,8},embedded).getPixelAt(0,0)==Colours::red,"missing PNG uses the embedded skin");
        write(bundle,shipped);write(user,edited);
        check(tonegrid::assets::loadSkinPng137(folders,"face01.png",{8,8},embedded).getPixelAt(0,0)==Colours::blue,"user PNG takes precedence over shipped and embedded art");
        user.replaceWithText("not a PNG");
        check(tonegrid::assets::loadSkinPng137(folders,"face01.png",{8,8},embedded).getPixelAt(0,0)==Colours::green,"corrupt user PNG safely falls back to shipped art");
        Image wrong(Image::ARGB,9,8,true);write(user,wrong);
        check(tonegrid::assets::loadSkinPng137(folders,"face01.png",{8,8},embedded).getPixelAt(0,0)==Colours::green,"changed canvas size cannot misalign the controls or jacks");
        write(user,embedded);
        check(tonegrid::assets::loadSkinPng137(folders,"face01.png",{8,8},edited).getPixelAt(0,0)==Colours::red,"replacing a PNG is read without rebuilding the executable");
        bundle.replaceWithText("corrupt");user.deleteFile();
        check(tonegrid::assets::loadSkinPng137(folders,"face01.png",{8,8},embedded).getPixelAt(0,0)==Colours::red,"both invalid external files fall back to the embedded skin");
    }
    tonegrid::PedalMidiLearnView learn("Classic Fuzz");int started=0,cleared=0,closed=0;
    learn.onLearn=[&]{++started;};learn.onClear=[&]{++cleared;};learn.onClose=[&]{++closed;};
    auto* start=dynamic_cast<Button*>(learn.findChildWithID("pedal-midi-relearn"));auto* clear=dynamic_cast<Button*>(learn.findChildWithID("pedal-midi-clear"));auto* close=dynamic_cast<Button*>(learn.findChildWithID("pedal-midi-close"));
    learn.update({},false,false);check(start&&!start->isEnabled()&&clear&&!clear->isEnabled(),"no MIDI input has an explicit setup state");
    learn.update({},true,true);check(close&&close->getButtonText()=="Cancel"&&!start->isEnabled(),"listening state has an accessible cancel action");save("midi-learn.png",learn.createComponentSnapshot(learn.getLocalBounds()));
    learn.update({1,5,1},false,true);start->onClick();clear->onClick();close->onClick();
    check(started==1&&cleared==1&&closed==1&&clear->isEnabled(),"learn, clear and close invoke the existing MIDI workflow callbacks once");
    report+=String(checks)+" pedal skin 1.1.37 checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
