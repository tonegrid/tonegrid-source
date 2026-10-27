#pragma once
#include "Platform/AppPaths.h"
#include "Modules/Learn/UI/LearnView.h"
class Learn132BrowserProbe final:public juce::Component,private juce::Timer
{
    class Window final:public juce::DocumentWindow
    {
    public:Window():DocumentWindow("ToneGrid Learn playback check",juce::Colours::black,DocumentWindow::closeButton){setUsingNativeTitleBar(true);}
        void closeButtonPressed()override{juce::JUCEApplication::getInstance()->systemRequestedQuit();}
    };
public:
    Learn132BrowserProbe(juce::File destination,std::function<void(bool)> complete):output(std::move(destination)),finished(std::move(complete))
    {
        tonegrid::learn::Partner partner;tonegrid::learn::decode(juce::String::fromUTF8(ToneGridApplicationAssets::lessons_json,ToneGridApplicationAssets::lessons_jsonSize),lessons,partner);
        profile=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("ToneGrid-learn132-browser-"+juce::Uuid().toString());const auto resources=tonegrid::paths::installed().resources().getChildFile("Learn");
        addAndMakeVisible(view);view.configure(resources.getChildFile("lessons.json"),profile.getChildFile("favourites.json"),profile.getChildFile("WebView2"));view.openExternal=[](const juce::String&){return false;};setSize(1120,780);view.setBounds(getLocalBounds());window.setContentNonOwned(this,true);window.centreWithSize(1120,808);window.setVisible(true);originalBounds=window.getBounds();beginStage();startTimer(500);
    }
    ~Learn132BrowserProbe()override{stopTimer();view.stopPlayer();window.clearContentComponent();}
    void resized()override{view.setBounds(getLocalBounds());}
private:
    void beginStage()
    {
        const juce::String titles[]{"Day Tripper","Have You Ever Seen The Rain","Day Tripper","Gravity","Gravity","Day Tripper"};
        auto found=std::find_if(lessons.begin(),lessons.end(),[&](const auto& item){return item.title==titles[stage];});if(found==lessons.end()){finish(false);return;}
        if(stage==2)view.closeLesson();if(stage==5){view.openLesson(lessons.front().id);view.closeLesson();}
        if(stage!=4)view.openLesson(found->id);else view.selectPartForTest(1);expected=tonegrid::jam::videoId(found->videos[size_t(stage==4?1:0)].url);attempt=0;
    }
    void timerCallback()override
    {
        ++attempt;view.inspectPlayer([safe=juce::Component::SafePointer<Learn132BrowserProbe>(this)](juce::var value)
        {
            if(!safe||safe->done)return;safe->last=value.toString();const auto json=juce::JSON::parse(safe->last);const auto* videos=json["videos"].getArray();bool playing=false;if(videos)for(const auto& video:*videos)playing|=!bool(video["paused"])&&double(video["time"])>.2;
            const double ratio=double(json["width"])/juce::jmax(1.,double(json["height"]));double time=0;if(videos&&!videos->isEmpty())time=double(videos->getReference(0)["time"]);
            if(safe->fullStep>0)
            {
                if(!playing||json["videoId"].toString()!=safe->expected||int(json["error"])!=0||time<safe->beforeFullscreen)return;
                const auto* display=juce::Desktop::getInstance().getDisplays().getDisplayForRect(safe->window.getScreenBounds());
                // AppKit excludes the camera housing on a notched MacBook.
                // Windows has zero safe-area insets, so its exact check is unchanged.
                const bool coversMonitor=display&&safe->window.getScreenBounds()==display->safeAreaInsets.subtractedFrom(display->totalArea);
                if(safe->fullStep==1&&safe->view.playerFullScreen()&&coversMonitor&&int(json["width"])==safe->window.getWidth()&&int(json["height"])==safe->window.getHeight())
                {safe->history+="PASS fullscreen enter, same video and continuing playback "+safe->last+"\n";safe->beforeFullscreen=time;safe->view.sendFullscreenEventForTest(false);safe->fullStep=2;safe->attempt=0;return;}
                if(safe->fullStep==2&&!safe->view.playerFullScreen()&&!safe->window.getPeer()->isFullScreen()&&safe->window.getBounds()==safe->originalBounds&&std::abs(ratio-16./9.)<.01)
                {safe->history+="PASS fullscreen exit, original window and continuing playback "+safe->last+"\n";safe->view.sendFullscreenEventForTest(true);safe->fullStep=3;safe->attempt=0;return;}
                if(safe->fullStep==3&&safe->view.playerFullScreen()&&coversMonitor)
                {safe->view.escapeFullscreenForTest();safe->fullStep=4;safe->attempt=0;return;}
                if(safe->fullStep==4&&!safe->view.playerFullScreen()&&safe->window.getBounds()==safe->originalBounds&&std::abs(ratio-16./9.)<.01)
                {safe->history+="PASS Escape restores original window and continues playback\n";safe->view.sendFullscreenEventForTest(true);safe->fullStep=5;safe->attempt=0;return;}
                if(safe->fullStep==5&&safe->view.playerFullScreen()&&coversMonitor)
                {safe->view.stopPlayer();if(safe->view.playerFullScreen()||safe->window.getPeer()->isFullScreen()||safe->window.getBounds()!=safe->originalBounds){safe->finish(false);return;}safe->history+="PASS stop in fullscreen restores window and stops playback\n";safe->finish(true);return;}
                return;
            }
            if(playing&&json["videoId"].toString()==safe->expected&&int(json["error"])==0&&int(json["height"])>200&&std::abs(ratio-16./9.)<.01)
            {safe->history+="PASS stage "+juce::String(safe->stage)+" "+safe->last+"\n";if(safe->stage==5){safe->beforeFullscreen=time;safe->fullStep=1;safe->attempt=0;safe->view.sendFullscreenEventForTest(true);}else{++safe->stage;safe->beginStage();}}
        });
        if(attempt>=60||view.playerNetworkError().isNotEmpty())finish(false);
    }
    void finish(bool ok)
    {
        if(done)return;done=true;stopTimer();const auto failure=view.playerNetworkError();
        const auto* display=juce::Desktop::getInstance().getDisplays().getDisplayForRect(window.getScreenBounds());
        const auto geometry="fullscreen-step "+juce::String(fullStep)+" active "+juce::String(int(view.playerFullScreen()))+" window "+window.getScreenBounds().toString()+" display "+(display?display->totalArea.toString():"unavailable")+" safe-area "+(display?display->safeAreaInsets.subtractedFrom(display->totalArea).toString():"unavailable");
        view.stopPlayer();window.setVisible(false);output.getParentDirectory().createDirectory();output.replaceWithText(juce::String(ok?"PASS":"FAIL")+" native Learn playback, large 16:9 player\n"+history+"stage "+juce::String(stage)+" "+failure+"\n"+geometry+"\n"+last+"\n");if(finished)finished(ok);
    }
    juce::File output,profile;std::function<void(bool)> finished;Window window;tonegrid::learn::View view;std::vector<tonegrid::learn::Lesson> lessons;juce::String expected,last,history;juce::Rectangle<int> originalBounds;double beforeFullscreen=0;int attempt=0,stage=0,fullStep=0;bool done=false;
};
