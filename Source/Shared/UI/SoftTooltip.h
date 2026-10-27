#pragma once
#include "Shared/UI/UiStyle.h"
namespace tonegrid
{
// One tooltip surface for the host and all its popups, with no focus stealing.
class SoftTooltip final:public juce::Component,private juce::Timer
{
public:
    SoftTooltip(juce::Component*,int delay):waitMs(delay){setInterceptsMouseClicks(false,false);setWantsKeyboardFocus(false);setAlwaysOnTop(true);startTimerHz(60);}
    void paint(juce::Graphics& g)override
    {
        auto r=getLocalBounds().toFloat().reduced(1);g.setColour(juce::Colour(0xff1c292f));g.fillRoundedRectangle(r,9);
        g.setColour(juce::Colour(0xff697b82).withAlpha(.28f));g.drawRoundedRectangle(r,9,.65f);
        layout.draw(g,getLocalBounds().toFloat().reduced(14,11));
    }
private:
    void timerCallback()override
    {
        auto mouse=juce::Desktop::getInstance().getMainMouseSource();auto* c=mouse.getComponentUnderMouse();juce::String tip;
        if(!mouse.isDragging())for(auto* p=c;p;p=p->getParentComponent())if(auto* client=dynamic_cast<juce::TooltipClient*>(p))if((tip=client->getTooltip()).isNotEmpty())break;
        const auto position=mouse.getScreenPosition().roundToInt();const auto now=juce::Time::getMillisecondCounter();
        if(tip!=pending||position.getDistanceFrom(anchor)>5){pending=tip;anchor=position;since=now;}
        const bool show=pending.isNotEmpty()&&now-since>=juce::uint32(waitMs);
        if(show&&pending!=shown)
        {
            shown=pending;juce::AttributedString text;text.append(shown,ui::font(14),juce::Colour(0xfff1f4f6));layout.createLayout(text,390);
            const int w=juce::roundToInt(layout.getWidth())+30,h=juce::roundToInt(layout.getHeight())+24;
            auto area=juce::Desktop::getInstance().getDisplays().getDisplayForPoint(position)->userArea;
            auto bounds=juce::Rectangle<int>(position.x+14,position.y+20,w,h).constrainedWithin(area.reduced(8));setBounds(bounds);
            if(!isOnDesktop())addToDesktop(juce::ComponentPeer::windowIsTemporary|juce::ComponentPeer::windowIgnoresMouseClicks);
            repaint();
        }
        const float target=show?1.f:0.f;opacity+=juce::jlimit(-.14f,.09f,target-opacity);setAlpha(opacity);
        setVisible(opacity>.001f);if(opacity<=.001f)shown.clear();
    }
    int waitMs;juce::uint32 since=0;juce::Point<int> anchor;juce::String pending,shown;juce::TextLayout layout;float opacity=0;
};
}
