#pragma once
#include "Shared/UI/UiMotion138.h"
#include "Modules/Play/UI/PedalAssetSkin.h"
#include "Modules/Jam/UI/JamView.h"
namespace uiMotion138Tests
{
inline bool run(juce::String& report)
{
    using namespace juce;bool ok=true;int count=0;
    auto check=[&](bool pass,const String& what){ok&=pass;++count;report+=String(pass?"PASS ":"FAIL ")+what+"\n";};
    bool bounded=true,periodic=true;for(int i=0;i<1000;++i)
    {
        const double t=i*.017;const float alpha=tonegrid::motion::ledOpacity(t),accent=tonegrid::motion::tabAccent(t);
        bounded&=alpha>=.75f&&alpha<=1.f&&accent>=0&&accent<=1;
        periodic&=std::abs(alpha-tonegrid::motion::ledOpacity(t+3))<1e-5f&&std::abs(accent-tonegrid::motion::tabAccent(t+12))<1e-5f;
    }
    check(bounded&&periodic,"animations stay bounded; LED repeats at three seconds and tab at twelve seconds");
    check(tonegrid::motion::ledOpacity(0)==1.f&&tonegrid::motion::ledOpacity(1.5)==.75f,"LED breath reaches 100 and 75 percent without switching off");
    check(tonegrid::motion::tabAccent(0)==0&&tonegrid::motion::tabAccent(9.99)==0&&tonegrid::motion::tabAccent(10)==0&&tonegrid::motion::tabAccent(11)==1&&tonegrid::motion::tabAccent(12)==0,"tab stays white for ten seconds then breathes grey and white for two seconds");
    tonegrid::motion::HoverValueSlider slider;slider.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);slider.setTextBoxStyle(Slider::TextBoxBelow,false,65,17);slider.setSize(74,56);
    auto alpha=[&](){for(auto* c:slider.getChildren())if(auto* label=dynamic_cast<Label*>(c))return label->getAlpha();return -1.f;};
    check(alpha()==0&&slider.readoutOpacity()==0,"real slider readout starts hidden, without changing its value or hit target");
    slider.advance(true,1./30.);const auto first=alpha();check(first>0&&first<1,"hover begins with an intermediate opacity");
    for(int i=0;i<20;++i)slider.advance(true,1./30.);check(alpha()==1,"nearby mouse reveals the readout fully");
    slider.advance(false,1./30.);check(alpha()>0&&alpha()<1,"leaving begins a smooth fade out");
    for(int i=0;i<20;++i)slider.advance(false,1./30.);check(alpha()==0,"readout disappears completely after leaving");
    const auto& skin=tonegrid::assets::pedal137(0);Image bright(Image::ARGB,160,160,true),dim(Image::ARGB,160,160,true);
    {Graphics g(bright);skin.paintLed(g,{0,0,140,140},true,1.f);}{Graphics g(dim);skin.paintLed(g,{0,0,140,140},true,.75f);}
    int lensChanges=0,bezelChanges=0;
    for(int y=0;y<160;++y)for(int x=0;x<160;++x)if(bright.getPixelAt(x,y)!=dim.getPixelAt(x,y))
    {const float dx=x-25.05f*3.5f,dy=y-21.2f*3.5f;const float radius=std::sqrt(dx*dx+dy*dy);if(radius<42.f)++lensChanges;else ++bezelChanges;}
    check(lensChanges>100&&bezelChanges==0,"LED animation changes lens pixels only; bezel and shadow remain fixed");
    tonegrid::jam::Tab tab("PLAY");tab.setSize(52,24);tab.setToggleState(false,dontSendNotification);tab.animate(0);const auto inactive=tab.createComponentSnapshot(tab.getLocalBounds());tab.animate(.25);const auto still=tab.createComponentSnapshot(tab.getLocalBounds());
    bool unchanged=true;for(int y=0;y<24;++y)for(int x=0;x<52;++x)unchanged&=inactive.getPixelAt(x,y)==still.getPixelAt(x,y);
    check(unchanged,"inactive tabs remain visually still");
    tab.setToggleState(true,dontSendNotification);tab.animate(100);const auto start=tab.createComponentSnapshot(tab.getLocalBounds());
    tab.animate(109.99);const auto quiet=tab.createComponentSnapshot(tab.getLocalBounds());tab.animate(111);const auto grey=tab.createComponentSnapshot(tab.getLocalBounds());tab.animate(112);const auto white=tab.createComponentSnapshot(tab.getLocalBounds());
    bool sameQuiet=true,sameWhite=true;int changes=0;
    for(int y=0;y<24;++y)for(int x=0;x<52;++x){sameQuiet&=start.getPixelAt(x,y)==quiet.getPixelAt(x,y);sameWhite&=start.getPixelAt(x,y)==white.getPixelAt(x,y);changes+=start.getPixelAt(x,y)!=grey.getPixelAt(x,y);}
    check(sameQuiet&&sameWhite&&changes>10,"active tab restarts its quiet interval on activation; only its text breathes");
    check(start.getPixelAt(1,1).getAlpha()==0&&grey.getPixelAt(1,1).getAlpha()==0,"tab has no rectangular outline");
    report+=String(count)+" UI motion 1.1.38 checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
