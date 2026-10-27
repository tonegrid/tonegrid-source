#pragma once
#include "SessionRecorder.h"
#include "Shared/UI/UiStyle.h"
#include <array>

namespace tonegrid::recording
{
class IconButton final : public juce::Button
{
public:
    enum Kind { recordIcon,playIcon,exportIcon };
    explicit IconButton(Kind value):Button(value==recordIcon?"Record":value==playIcon?"Play":"Export MP3"),kind(value)
    {setTitle(getName());}
    bool recording=false,playing=false;float pulse=1;
private:
    void paintButton(juce::Graphics& g,bool hover,bool down) override
    {
        const auto r=getLocalBounds().toFloat().reduced(4);const auto centre=r.getCentre();
        g.setColour(tonegrid::ui::panel().withAlpha(.75f));g.fillEllipse(r);
        auto colour=recording?juce::Colour(0xffef5050):kind==playIcon?juce::Colour(0xff149dff):tonegrid::ui::secondaryInk();
        colour=colour.withAlpha(!isEnabled()?tonegrid::ui::disabledOpacity:recording?pulse:down?.65f:hover?1.f:.85f);
        g.setColour(colour);g.drawEllipse(r,tonegrid::ui::stroke);const float size=r.getWidth()*.29f;
        if(kind==recordIcon)g.fillEllipse(centre.x-size/2,centre.y-size/2,size,size);
        else if(kind==playIcon)
        {
            if(playing)g.fillRect(juce::Rectangle<float>(size,size).withCentre(centre));
            else {juce::Path p;p.addTriangle(centre.x-size*.4f,centre.y-size*.65f,centre.x-size*.4f,centre.y+size*.65f,centre.x+size*.7f,centre.y);g.fillPath(p);}
        }
        else
        {
            juce::Path p;p.addRoundedRectangle(centre.x-size*.7f,centre.y-size*.65f,size*1.4f,size*1.3f,2);
            p.startNewSubPath(centre.x,centre.y-size);p.lineTo(centre.x,centre.y+size*.1f);
            p.startNewSubPath(centre.x-size*.3f,centre.y-size*.2f);p.lineTo(centre.x,centre.y+size*.1f);p.lineTo(centre.x+size*.3f,centre.y-size*.2f);
            g.strokePath(p,juce::PathStrokeType(1.5f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        }
    }
    Kind kind;
};
class Controls final : public juce::Component
{
public:
    explicit Controls(SessionRecorder& source):engine(source)
    {
        for(auto* b:{&rec,&play,&exportButton})addAndMakeVisible(b);
        tonegrid::l10n::tooltip(rec,"Record the sound of ToneGrid. Click again to stop. Maximum 10 minutes.");
        tonegrid::l10n::tooltip(play,"Play the last recording. Click again to stop.");tonegrid::l10n::tooltip(exportButton,"Export the last recording as MP3.");
        addAndMakeVisible(time);time.setJustificationType(juce::Justification::centred);time.setFont(tonegrid::ui::font(tonegrid::ui::statusSize));time.setColour(juce::Label::textColourId,tonegrid::ui::secondaryInk());time.setInterceptsMouseClicks(false,false);
        rec.onClick=[this]{if(engine.state()==Mode::recording)engine.stop();else if(onRecord)onRecord();sync();};
        play.onClick=[this]{if(engine.state()==Mode::playing)engine.stop();else if(onPlay)onPlay();sync();};
        exportButton.onClick=[this]{if(onExport)onExport();};sync();
    }
    std::function<void()> onRecord,onPlay,onExport;
    void setAudioAvailable(bool available){audioAvailable=available;sync();}
    void setLoopActive(bool value){loopActive=value;sync();}
    void setChoosingFile(bool value){choosingFile=value;sync();}
    void sync()
    {
        const auto state=engine.state();const bool idle=state==Mode::empty||state==Mode::ready;
        const bool blocked=engine.requestPending()||choosingFile;
        rec.recording=state==Mode::recording;play.playing=state==Mode::playing;
        rec.setEnabled(!blocked&&audioAvailable&&(idle||rec.recording||play.playing));
        play.setEnabled(!loopActive&&!blocked&&audioAvailable&&(state==Mode::ready||play.playing));
        exportButton.setEnabled(!blocked&&state==Mode::ready);
        rec.pulse=.35f+.65f*float((1+std::sin(juce::Time::getMillisecondCounterHiRes()*.0062831853))/2);
        tonegrid::l10n::text(time,juce::String(int(std::floor(engine.seconds())))+" s",juce::dontSendNotification);
        time.setVisible(rec.recording||engine.available()||state==Mode::finishing);rec.repaint();play.repaint();
    }
    std::array<juce::Rectangle<int>,3> actionBounds()const{return {play.getBounds(),rec.getBounds(),exportButton.getBounds()};}
    void resized() override
    {
        const int side=juce::jmin(46,getWidth()),x=(getWidth()-side)/2;
        play.setBounds(x,0,side,side);time.setBounds(0,2*side+20,getWidth(),20);
        rec.setBounds(x,side+14,side,side);exportButton.setBounds(x,2*side+42,side,side);
    }
private:
    SessionRecorder& engine;IconButton rec{IconButton::recordIcon},play{IconButton::playIcon},exportButton{IconButton::exportIcon};juce::Label time;
    bool audioAvailable=false,choosingFile=false,loopActive=false;
};
}
