#pragma once
#include "LoopRecorder.h"
#include "Shared/UI/UiStyle.h"
namespace tonegrid::looping
{
class Controls final:public juce::Button
{
public:
    explicit Controls(Engine& source):Button("Looper"),engine(source)
    {setComponentID("looper");setTitle("Looper");tonegrid::l10n::tooltip(*this,"Click: record / loop / stop / resume. Hold to clear. One 60-second loop, no overdub. The loop keeps its recorded sound.");}
    std::function<void()> onAdvance;
    void sync(bool available){setEnabled(available&&!engine.pending());repaint();}
    void mouseDown(const juce::MouseEvent& e) override{pressedAt=juce::Time::getMillisecondCounterHiRes();Button::mouseDown(e);}
    void mouseUp(const juce::MouseEvent& e) override
    {held=juce::Time::getMillisecondCounterHiRes()-pressedAt>=700;Button::mouseUp(e);held=false;}
    void clicked() override{if(held)engine.clear();else if(onAdvance)onAdvance();held=false;}
private:
    void paintButton(juce::Graphics& g,bool hover,bool down) override
    {
        using namespace juce;const auto bounds=getLocalBounds().toFloat();const float diameter=jmin(bounds.getWidth(),bounds.getHeight())-8;const auto r=bounds.withSizeKeepingCentre(diameter,diameter);const auto m=engine.state();
        g.setColour(tonegrid::ui::panel().withAlpha(.75f));g.fillEllipse(r);
        auto colour=m==Mode::recording?Colour(0xffef5050):m==Mode::playing?Colour(0xff40ddb0):tonegrid::ui::secondaryInk();
        g.setColour(colour.withAlpha(!isEnabled()?tonegrid::ui::disabledOpacity:down?.6f:hover?1.f:.85f));g.drawEllipse(r,tonegrid::ui::stroke);
        auto icon=r.reduced(r.getWidth()*.27f);Path path;
        path.addCentredArc(icon.getCentreX(),icon.getCentreY(),icon.getWidth()*.5f,icon.getHeight()*.5f,0,-.8f,2.3f,true);
        path.addCentredArc(icon.getCentreX(),icon.getCentreY(),icon.getWidth()*.5f,icon.getHeight()*.5f,0,2.34f,5.44f,true);
        g.strokePath(path,PathStrokeType(1.5f,PathStrokeType::curved,PathStrokeType::rounded));
        path.clear();path.addTriangle(icon.getRight(),icon.getCentreY()-3,icon.getRight()-5,icon.getCentreY()-7,icon.getRight()+2,icon.getCentreY()-8);
        path.addTriangle(icon.getX(),icon.getCentreY()+3,icon.getX()+5,icon.getCentreY()+7,icon.getX()-2,icon.getCentreY()+8);g.fillPath(path);
        if(m!=Mode::empty){g.setFont(tonegrid::ui::font(tonegrid::ui::microSize));g.drawText(String(int(engine.seconds()))+tonegrid::l10n::translate("s"),getLocalBounds().removeFromBottom(11).translated(0,-4),Justification::centred);}
    }
    Engine& engine;double pressedAt=0;bool held=false;
};
}
