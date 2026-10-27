#pragma once
#include "Shared/Media/VideoPlayer.h"
#include <ToneGridApplicationAssets.h>
class Jam131BrowserProbe final:public juce::Component,private juce::Timer
{
    class Window final:public juce::DocumentWindow
    {
    public:Window():DocumentWindow("ToneGrid inline player check",juce::Colours::black,DocumentWindow::closeButton){setUsingNativeTitleBar(true);}
        void closeButtonPressed()override{juce::JUCEApplication::getInstance()->systemRequestedQuit();}
    };
public:
    Jam131BrowserProbe(juce::File destination,std::function<void(bool)> complete):output(std::move(destination)),finished(std::move(complete))
    {
        tonegrid::jam::decodeCatalogue(juce::String::fromUTF8(ToneGridApplicationAssets::backingtracks_json,ToneGridApplicationAssets::backingtracks_jsonSize),tracks);
        profile=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("ToneGrid-jam131-browser-"+juce::Uuid().toString());
        addAndMakeVisible(player);player.configure(profile);setSize(380,220);player.setBounds(12,10,356,200);window.setContentNonOwned(this,true);window.centreWithSize(380,248);window.setVisible(true);
        if(!tracks.empty())player.start(tracks[0]);player.showPlayer(true);startTimer(500);
    }
    ~Jam131BrowserProbe()override{stopTimer();player.stop();window.clearContentComponent();}
    void resized()override{player.setBounds(12,10,356,200);}
private:
    void timerCallback()override
    {
        ++attempt;player.inspect([safe=juce::Component::SafePointer<Jam131BrowserProbe>(this)](juce::var value)
        {
            if(!safe)return;safe->last=value.toString();const auto json=juce::JSON::parse(safe->last);const auto* videos=json["videos"].getArray();bool playing=false;if(videos)for(const auto& video:*videos)playing|=!bool(video["paused"])&&double(video["time"])>.2;
            if(playing&&int(json["error"])==0&&json["videoId"].toString()==safe->player.currentId()&&int(json["width"])==356&&int(json["height"])==200)
            {
                safe->history+="PASS stage "+juce::String(safe->stage)+" "+safe->player.currentId()+" "+safe->last+"\n";
                if(++safe->stage==6){safe->finish(true);return;}
                safe->attempt=0;
                const int selection[]={0,1,281,0,2,0};
                if(safe->stage==3)safe->player.stop();
                if(safe->stage==4){safe->player.start(safe->tracks[1]);safe->player.showPlayer(true);}
                safe->player.start(safe->tracks[size_t(selection[safe->stage])]);safe->player.showPlayer(true);
            }
        });
        if(attempt>=50||player.lastNetworkError().isNotEmpty())finish(false);
    }
    void finish(bool ok)
    {
        if(done)return;done=true;stopTimer();const auto failure=player.lastNetworkError();player.stop();window.setVisible(false);output.getParentDirectory().createDirectory();output.replaceWithText(juce::String(ok?"PASS":"FAIL")+" native WebView2 sequential playback at 200px\n"+history+"stage "+juce::String(stage)+" "+failure+"\n"+last+"\n");if(finished)finished(ok);
    }
    juce::File output,profile;std::function<void(bool)> finished;Window window;tonegrid::media::Player player;std::vector<tonegrid::jam::Track> tracks;juce::String last,history;int attempt=0,stage=0;bool done=false;
};
