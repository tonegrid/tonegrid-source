#pragma once
#include "Shared/UI/UiStyle.h"
#include "Shared/UI/UiMotion138.h"
#include <vector>
namespace tonegrid::libraryui
{
// Keep artwork hit targets fully opaque for JUCE mouse routing. Hide the
// button chrome in paint, rather than making the interactive component transparent.
class ArtworkButton final:public juce::TextButton
{
public:
    ArtworkButton(){setMouseCursor(juce::MouseCursor::PointingHandCursor);}
    void paintButton(juce::Graphics& g,bool,bool)override
    {
        if(hasKeyboardFocus(true))
        {g.setColour(ui::secondaryInk().withAlpha(.6f));g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1),3,.8f);}
    }
};
class CloseLibraryButton final:public juce::TextButton
{
public:
    explicit CloseLibraryButton(const juce::String& id):TextButton("OK")
    {
        setComponentID(id);l10n::tooltip(*this,"Close library");
        onClick=[this]{if(auto* window=findParentComponentOfClass<juce::DialogWindow>())window->closeButtonPressed();};
    }
    void paintButton(juce::Graphics& g,bool hover,bool down)override
    {
        const auto blue=ui::tabBlue();const auto area=getLocalBounds().toFloat().reduced(.5f);
        const auto fill=down?blue.darker(.2f):hover?blue.brighter(.12f):blue;
        g.setColour(fill.withAlpha(isEnabled()?1.f:ui::disabledOpacity));g.fillRoundedRectangle(area,6);
        g.setColour(juce::Colours::white.withAlpha(hasKeyboardFocus(true)?.8f:.24f));g.drawRoundedRectangle(area,6,1);
        g.setColour(juce::Colours::white.withAlpha(isEnabled()?1.f:ui::disabledOpacity));g.setFont(ui::font(ui::controlSize));
        g.drawText(getButtonText(),getLocalBounds().reduced(10,2),juce::Justification::centred);
    }
};
inline void sidebarRow(juce::Graphics& g,int w,int h,const juce::String& title,int count,bool selected,bool favorite)
{
    using namespace juce;
    if(selected){g.setColour(Colour(0xff123756));g.fillRoundedRectangle(0,2,float(w),float(h-4),6);}
    g.setColour(selected?Colour(0xff2098f4):ui::secondaryInk());Path icon;
    if(favorite)icon.addStar({22,float(h/2)},5,5,11);
    else{const float y=float(h/2-9);icon.startNewSubPath(12,y+3);icon.lineTo(12,y-2);icon.lineTo(21,y-2);icon.lineTo(25,y+3);icon.lineTo(34,y+3);icon.lineTo(34,y+19);icon.lineTo(12,y+19);icon.closeSubPath();icon.startNewSubPath(12,y+3);icon.lineTo(34,y+3);}
    g.strokePath(icon,PathStrokeType(1.5f));g.setColour(ui::ink());g.setFont(ui::font(16));g.drawText(title,44,0,w-78,h,Justification::centredLeft,true);
    g.setColour(ui::metadataInk());g.setFont(ui::font(12));g.drawText(String(count),w-34,0,28,h,Justification::centredRight);
    if(favorite){g.setColour(ui::outline().withAlpha(.6f));g.drawHorizontalLine(h-1,4.f,float(w-4));}
}
// One presentation and interaction policy for Amp and Cabinet. The artwork hit
// target stays opaque; only the separate scrim and its controls fade.
class GearCard:public juce::Component,private juce::Timer
{
    class Face final:public juce::Component
    {
    public:explicit Face(GearCard& card):owner(card){setInterceptsMouseClicks(false,false);setBufferedToImage(true);}
        void paint(juce::Graphics& g)override{owner.paintFace(g);owner.paintFaceOverlay(g);}
    private:GearCard& owner;
    };
    class Scrim final:public juce::Component
    {void paint(juce::Graphics& g)override{g.setColour(juce::Colour(0xff141a1e).withAlpha(.88f));g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(1),10);}};
public:
    explicit GearCard(juce::String title):heading(std::move(title)),languageSubscription([this]{if(localiseModel)browse.setTooltip(l10n::translate(model));face.repaint();})
    {
        addAndMakeVisible(face);addAndMakeVisible(browse);addChildComponent(scrim);addChildComponent(actions);
        scrim.setInterceptsMouseClicks(false,false);actions.setInterceptsMouseClicks(false,true);actions.addAndMakeVisible(load);
        tonegrid::l10n::button(load,"Load another amp");load.onClick=[this]{if(browse.isEnabled()&&browse.onClick)browse.onClick();};
        setRows({{&load}});startTimerHz(60);
    }
    ~GearCard() override{stopTimer();}
    void setPresentation(juce::Image next,juce::String title,bool active=true,bool statusText=false)
    {image=std::move(next);model=std::move(title);localiseModel=statusText;available=active;browse.setEnabled(active);load.setEnabled(active);browse.setTooltip(statusText?l10n::translate(model):model);face.repaint();}
    void setRows(std::vector<std::vector<juce::Component*>> value)
    {rows=std::move(value);for(auto& row:rows)for(auto* c:row)actions.addAndMakeVisible(c);resized();}
    void advanceHover(bool nearby,double dt)
    {
        // Mouse-click focus must never pin a hover layer after Normalize or Lock.
        const bool show=available&&(nearby||(keepVisible&&keepVisible()));
        const auto next=transition.advance(show,dt);if(next==opacity)return;opacity=next;
        scrim.setAlpha(opacity);actions.setAlpha(opacity);scrim.setVisible(opacity>0);actions.setVisible(opacity>0);
    }
    const juce::Image& presentedArtwork()const noexcept{return image;}
    float hoverOpacity()const{return opacity;}
    juce::Rectangle<int> artworkBounds()const{return getLocalBounds().reduced(12,6).withTrimmedBottom(36);}
    void resized()override
    {
        face.setBounds(getLocalBounds());browse.setBounds(getLocalBounds());scrim.setBounds(getLocalBounds());
        const int rowHeight=juce::jmin(28,juce::jmax(22,(getHeight()-20-juce::jmax(0,int(rows.size())-1)*5)/juce::jmax(1,int(rows.size()))));
        actions.setBounds(getLocalBounds().withSizeKeepingCentre(juce::jmin(220,juce::jmax(1,getWidth()-24)),int(rows.size())*(rowHeight+5)-5));
        auto r=actions.getLocalBounds();for(auto& row:rows){auto band=r.removeFromTop(rowHeight);r.removeFromTop(5);if(row.size()==2){auto help=band.removeFromRight(28);band.removeFromRight(5);row[0]->setBounds(band);row[1]->setBounds(help);}else if(!row.empty())row[0]->setBounds(band);}
    }
    void paintFace(juce::Graphics& g)
    {
        using namespace juce;g.setImageResamplingQuality(Graphics::highResamplingQuality);const auto r=getLocalBounds().toFloat().reduced(.5f);
        g.setColour(Colour(0xff0b141a));g.fillRoundedRectangle(r,10);
        g.setColour(ui::outline().withAlpha(available?.75f:.4f));g.drawRoundedRectangle(r,10,.8f);
        const auto art=artworkBounds();g.setOpacity(available?1.f:.4f);
        if(image.isValid())g.drawImageWithin(image,art.getX(),art.getY(),art.getWidth(),art.getHeight(),RectanglePlacement::centred);
        g.setOpacity(1);auto text=getLocalBounds().reduced(12,0).withTrimmedTop(getHeight()-36).withTrimmedRight(18);
        g.setColour(ui::secondaryInk());g.setFont(ui::font(12));g.drawText(heading,text.removeFromTop(15),Justification::centredLeft);
        g.setColour(ui::ink().withAlpha(available?1.f:.6f));g.setFont(ui::font(13));g.drawText(localiseModel?l10n::translate(model):model,text.withHeight(17),Justification::centredLeft,true);
        if(available){Path arrow;const float x=float(getWidth()-18),y=float(getHeight()-21);arrow.startNewSubPath(x-5,y-2);arrow.lineTo(x,y+3);arrow.lineTo(x+5,y-2);g.setColour(Colour(0xff22b6df));g.strokePath(arrow,PathStrokeType(1.4f,PathStrokeType::curved,PathStrokeType::rounded));}
    }
    ArtworkButton browse;tonegrid::l10n::TextButton load;juce::Component actions;std::function<bool()> keepVisible;
protected:
    virtual void paintFaceOverlay(juce::Graphics&){}
private:
    void timerCallback() override
    {
        const auto now=juce::Time::getMillisecondCounterHiRes()*.001;
        if(!isShowing()){lastFrame=0;return;}
        const auto dt=lastFrame>0?now-lastFrame:1./60.;lastFrame=now;
        advanceHover(getLocalBounds().contains(getMouseXYRelative()),dt);
    }
    juce::String heading,model;juce::Image image;Face face{*this};Scrim scrim;motion::CardFade transition;double lastFrame=0;bool available=true,localiseModel=false;float opacity=0;std::vector<std::vector<juce::Component*>> rows;l10n::Subscription languageSubscription;
};
inline void gearIcon(juce::Graphics& g,juce::Rectangle<float> r,bool full,juce::Colour colour)
{
    g.setColour(colour);auto head=r.withHeight(full?r.getHeight()*.36f:r.getHeight()*.62f).withCentre({r.getCentreX(),full?r.getY()+r.getHeight()*.18f:r.getCentreY()});
    g.drawRoundedRectangle(head,2,1.4f);for(int i=0;i<4;++i)g.fillEllipse(head.getX()+4+i*(head.getWidth()-9)/4,head.getBottom()-5,2,2);
    g.drawLine(head.getCentreX()-5,head.getY()-3,head.getCentreX()+5,head.getY()-3,1.3f);
    if(full){auto cab=r.withTrimmedTop(r.getHeight()*.43f);g.drawRoundedRectangle(cab,2,1.4f);for(int i=0;i<4;++i){const auto d=cab.getHeight()*.35f;g.drawEllipse(cab.getX()+cab.getWidth()*(i%2?.65f:.23f)-d/2,cab.getY()+cab.getHeight()*(i/2?.72f:.28f)-d/2,d,d,1);}}
}
class TypeChoice final:public juce::Button
{
public:
    TypeChoice(juce::String title,bool rig):Button(title),full(rig){setClickingTogglesState(true);setToggleState(true,juce::dontSendNotification);setTitle(title);}
    void paintButton(juce::Graphics& g,bool hover,bool)override
    {
        auto r=getLocalBounds().toFloat().reduced(2);g.setColour(getToggleState()?juce::Colour(0xff183547):ui::panel());g.fillRoundedRectangle(r,7);
        g.setColour((getToggleState()?juce::Colour(0xff678e9e):ui::outline()).withAlpha(hover?1.f:.65f));g.drawRoundedRectangle(r,7,.8f);
        auto tick=juce::Rectangle<float>(r.getX()+10,r.getCentreY()-7,14,14);g.setColour(ui::secondaryInk());g.drawRoundedRectangle(tick,2,1);
        if(getToggleState()){juce::Path p;p.startNewSubPath(tick.getX()+3,tick.getCentreY());p.lineTo(tick.getX()+6,tick.getBottom()-3);p.lineTo(tick.getRight()-2,tick.getY()+3);g.setColour(ui::ink());g.strokePath(p,juce::PathStrokeType(1.5f));}
        const bool compact=getHeight()<44;const auto icon=juce::Rectangle<float>(r.getX()+32,r.getCentreY()-(compact?12:15),compact?21:28,compact?24:30);gearIcon(g,icon,full,ui::ink());g.setColour(ui::ink());g.setFont(ui::font(getHeight()<44?12:getWidth()<170?13:16,juce::Font::bold));g.drawFittedText(getName(),getLocalBounds().withTrimmedLeft(getHeight()<44?59:72).reduced(2),juce::Justification::centredLeft,1);
    }
private:bool full;
};
class LockIcon final:public juce::Button
{
public:LockIcon():Button("Lock cabinet"){setClickingTogglesState(true);tonegrid::l10n::tooltip(*this,"Keep this cabinet when changing amp heads. Unlock it to follow the next head's recommended cabinet.");}
    void paintButton(juce::Graphics& g,bool hover,bool)override
    {
        auto r=getLocalBounds().toFloat().reduced(8);auto body=r.withTrimmedTop(r.getHeight()*.39f);g.setColour(hover?juce::Colours::white:ui::secondaryInk());g.drawRoundedRectangle(body,3,1.6f);
        auto shackle=r.reduced(r.getWidth()*.22f,0).withHeight(r.getHeight()*.65f);juce::Path p;
        p.addCentredArc(shackle.getCentreX()+(getToggleState()?0:4),shackle.getY()+shackle.getWidth()/2,shackle.getWidth()/2,shackle.getWidth()/2,0,-juce::MathConstants<float>::halfPi,juce::MathConstants<float>::halfPi,true);
        p.lineTo(shackle.getRight()+(getToggleState()?0:4),body.getY()+2);g.strokePath(p,juce::PathStrokeType(1.6f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        g.fillEllipse(body.getCentreX()-1.8f,body.getCentreY()-2.5f,3.6f,3.6f);g.drawLine(body.getCentreX(),body.getCentreY(),body.getCentreX(),body.getCentreY()+4,1.5f);
    }
};
}
