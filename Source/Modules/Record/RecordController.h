#pragma once
#include "Modules/Record/UI/RecordView.h"
#include "Core/RecordAudioEngine.h"
#include "Core/ToneService.h"
#include "Modules/Record/Audio/OfflineGuitarRenderer.h"
#include <future>
namespace tonegrid::record {
class RecordController final:private juce::Timer {
public:
 RecordController(RecordView&,core::ToneService,juce::File session,juce::File encoder);
 ~RecordController()override;
 void activate();void deactivate();void shutdown();
 void importBacking(const juce::File&);void exportAudio(juce::File,bool mix,bool selection=false);
 void selectTrack(int,bool unfreeze=false);void renderTrack(int,int switchAfter=-1,bool unfreezeAfter=false);void removeReference();
 core::RecordAudioEngine& audio(){return engine;}
 const RecordSession& model()const{return session;}
 bool isBusy()const{return busy;}
private:
 struct Job {juce::Result result=juce::Result::ok();RecordSession session;std::unique_ptr<core::TimelineAudio> audio;juce::String message;bool publish=false,restore=false,recovered=false,recordArmed=false;int switchAfter=-1;bool unfreezeAfter=false;};
 void timerCallback()override;void finishJob();void preparePlayback(bool start);void togglePlayback();
 void startRecording();void stop();void seek(juce::int64);void remove(juce::int64,juce::int64);void syncReference(bool);
 void videoStatus(const VideoStatus&);void cancelReference();void ensureReference();juce::int64 videoFrame(double)const;void referenceInterrupted(const juce::String&);
 void loadBacking();void chooseExport(bool mix,bool selection=false);void save();void update();void levels();void applyLevels();void captureEditable();
 void setMessage(const juce::String& value){view.status.setText(value,juce::dontSendNotification);view.status.setTooltip(value);}
 juce::String captureTone();
 RecordView& view;core::ToneService tone;RecordSessionStore store;juce::File encoder;
 RecordSession session;core::RecordAudioEngine engine;std::future<Job> job;
 juce::Component::SafePointer<juce::DialogWindow> switchDialog;
 std::unique_ptr<core::IRenderProcessor> renderer;std::unique_ptr<juce::FileChooser> chooser;
 std::atomic<bool> cancel{false};std::atomic<float> progress{0};
 bool active=false,loaded=false,busy=false,recordPending=false,dirty=false,playAfter=false,shuttingDown=false,recoveryBlocked=false,stopRequested=false,pendingToneRestore=false,wasPlaying=false;
 int autosaveTicks=0;juce::int64 recordingStart=0;double recordingRate=48000;
 bool referenceWanted=false,referenceAwaiting=false,referenceBuffering=false,referenceVisibleLast=false;
 double referenceDeadline=0;juce::int64 pendingVideoSeek=-1;bool pendingVideoPlay=false;VideoDriftGuard videoDrift;
};
}
