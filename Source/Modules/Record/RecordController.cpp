#include "Modules/Record/RecordController.h"
namespace tonegrid::record {
using namespace juce;
RecordController::RecordController(RecordView& v,core::ToneService t,File folder,File enc):view(v),tone(std::move(t)),store(std::move(folder)),encoder(std::move(enc)) {
 view.goStart.onClick=[this]{seek(0);};view.play.onClick=view.onSpace=[this]{togglePlayback();};view.record.onClick=[this]{startRecording();};view.stop.onClick=[this]{if(renderer)cancel=true;else stop();};
 view.timeline.onSeek=[this](int64 p){seek(p);};view.timeline.onDelete=[this](int64 a,int64 b){remove(a,b);};
 view.loadAudio.onClick=[this]{loadBacking();};view.removeBacking.onClick=[this]{if(busy||recordPending)return;session.backing.clear();dirty=true;preparePlayback(false);update();};
 view.exportGuitar.onClick=[this]{chooseExport(true,true);};view.exportMix.onClick=[this]{chooseExport(true);};
 view.guitarMute.onClick=view.secondMute.onClick=view.backingMute.onClick=[this]{levels();};view.guitarVolume.onValueChange=view.secondVolume.onValueChange=view.backingVolume.onValueChange=[this]{levels();};
 view.armFirst.onClick=view.firstHeader.onClick=[this]{selectTrack(0);};view.armSecond.onClick=view.secondHeader.onClick=[this]{selectTrack(1);};
 view.renderFirst.onClick=[this]{if(session.rendered.isNotEmpty())selectTrack(0,true);else renderTrack(0);};view.renderSecond.onClick=[this]{if(session.second.rendered.isNotEmpty())selectTrack(1,true);else renderTrack(1);};
 view.loadYoutube.onClick=[this]{if(busy||recordPending)return;engine.quiesce();cancelReference();session.cursor=engine.position();const auto id=youtubeId(view.youtubeURL.getText().trim());if(id.isEmpty()){setMessage("Enter a valid YouTube video link.");return;}session.videoId=id;session.videoURL="https://www.youtube.com/watch?v="+id;session.youtubeStart=0;session.referenceStart=0;view.revealReference();view.youtube.load(session.videoURL);syncReference(false);dirty=true;update();};
 view.removeYoutube.onClick=[this]{removeReference();};
 view.youtube.onStatus=[this](const VideoStatus& status){videoStatus(status);};
}
RecordController::~RecordController(){shutdown();}
String RecordController::captureTone(){return tone.capture&&tone.available&&tone.available()?tone.capture():track(session,session.editableTrack>=0?session.editableTrack:session.activeTrack).tone;}
void RecordController::captureEditable(){if(!pendingToneRestore&&session.editableTrack>=0)track(session,session.editableTrack).tone=captureTone();}
void RecordController::activate(){
 active=true;engine.setSyncHeld(false);engine.setEnabled(true);startTimerHz(30);
 if(!loaded&&!busy){busy=true;job=std::async(std::launch::async,[this]{Job j;j.result=store.load(j.session);j.publish=j.result.wasOk();j.restore=j.publish&&j.session.editableTrack>=0&&track(j.session,j.session.editableTrack).tone.isNotEmpty();
  if(j.result.wasOk()&&j.session.pendingTake.isNotEmpty()){
   AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<AudioFormatReader> r(formats.createReaderFor(store.folder.getChildFile(j.session.pendingTake)));
   if(r&&r->lengthInSamples>0){commitTake(j.session,{j.session.pendingTake,j.session.pendingStart,std::llround(r->lengthInSamples*j.session.rate/r->sampleRate),0});j.message="Recovered the last flushed part of an interrupted take. All source recordings were preserved.";}else j.message="An interrupted take could not be recovered automatically. Its source file was preserved.";
   j.session.pendingTake.clear();j.recovered=true;
  }
  if(j.result.wasOk())j.result=loadSessionAudio(store.folder,j.session,j.audio);return j;
 });}else if(loaded){if(pendingToneRestore&&tone.restore&&session.editableTrack>=0){const auto restored=tone.restore(track(session,session.editableTrack).tone);pendingToneRestore=restored.failed();if(restored.failed())setMessage("Session audio is intact, but its saved tone could not load: "+restored.getErrorMessage());}update();ensureReference();}
}
void RecordController::deactivate(){active=false;playAfter=false;pendingVideoSeek=-1;pendingVideoPlay=false;referenceWanted=referenceAwaiting=referenceBuffering=false;if(switchDialog)switchDialog->closeButtonPressed();view.dismissHelp();view.youtube.pause();if(recordPending)stop();else if(!busy){engine.stop();session.cursor=engine.position();captureEditable();dirty=true;save();}engine.setEnabled(false);engine.setSyncHeld(false);startTimerHz(30);}
void RecordController::shutdown(){
 if(shuttingDown)return;view.youtube.onStatus={};shuttingDown=true;active=false;cancel=true;stopTimer();if(switchDialog)switchDialog->closeButtonPressed();view.dismissHelp();view.youtube.pause();chooser.reset();engine.quiesce();
 if(job.valid()){job.wait();finishJob();}
 if(recordPending){const auto frames=engine.stop();if(frames>0)commitTake(session,{session.pendingTake,recordingStart,(juce::int64)std::floor(frames*session.rate/recordingRate),0});session.pendingTake.clear();recordPending=false;}
 if(loaded&&!recoveryBlocked){session.cursor=engine.position();captureEditable();store.save(session);}engine.setEnabled(false);renderer.reset();
}
void RecordController::applyLevels(){const auto& t=track(session,session.editableTrack>=0?session.editableTrack:session.activeTrack);engine.controls(t.guitarVolume,session.backingVolume,session.editableTrack<0||t.guitarMute,session.backingMute);engine.renderedControls(session.guitarMute?0:session.guitarVolume,session.second.guitarMute?0:session.second.guitarVolume);}
void RecordController::levels(){session.guitarVolume=(float)view.guitarVolume.getValue();session.second.guitarVolume=(float)view.secondVolume.getValue();session.backingVolume=(float)view.backingVolume.getValue();session.guitarMute=view.guitarMute.getToggleState();session.second.guitarMute=view.secondMute.getToggleState();session.backingMute=view.backingMute.getToggleState();applyLevels();dirty=true;}
void RecordController::update(){view.setState(session,store.folder);applyLevels();}
void RecordController::finishJob(){
 if(!job.valid()||job.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
 Job result;try{result=job.get();}catch(const std::exception& e){result.result=Result::fail("Record operation failed: "+String(e.what()));}busy=false;renderer.reset();
 if(result.publish){session=std::move(result.session);loaded=true;dirty=dirty||!result.restore||result.recovered;engine.setTimeline(std::move(result.audio));engine.seek(session.cursor);update();}
 if(result.restore)pendingToneRestore=true;
 if(result.restore&&active&&tone.restore&&result.result.wasOk()&&session.editableTrack>=0){auto restored=tone.restore(track(session,session.editableTrack).tone);pendingToneRestore=restored.failed();if(restored.failed()){setMessage("Session audio recovered, but its saved tone could not load: "+restored.getErrorMessage());playAfter=false;return;}}
 if(result.result.failed()){playAfter=false;if(recordPending&&engine.state()!=core::RecordAudioEngine::recording){engine.stop();recordPending=false;session.pendingTake.clear();}setMessage(result.result.getErrorMessage());if(!loaded){recoveryBlocked=true;loaded=true;}return;}
 if(result.message.isNotEmpty())setMessage(result.message);
 if(result.restore&&active&&session.videoId.isNotEmpty()){ensureReference();syncReference(false);}
 if(stopRequested&&!shuttingDown){stopRequested=false;stop();return;}
 if(playAfter&&active){playAfter=false;engine.setSyncHeld(session.videoId.isNotEmpty());if(engine.play())syncReference(true);}
 if(result.recordArmed&&recordPending&&engine.state()==core::RecordAudioEngine::recording&&active)syncReference(true);
 if(result.switchAfter>=0&&active&&!shuttingDown){selectTrack(result.switchAfter,result.unfreezeAfter);return;}
 if(active&&!busy&&pendingVideoSeek>=0){const auto target=pendingVideoSeek;const bool start=pendingVideoPlay;pendingVideoSeek=-1;pendingVideoPlay=false;engine.seek(target);session.cursor=target;dirty=true;view.timeline.revealPosition(target);if(start)preparePlayback(true);else syncReference(false);return;}
 if(!active&&!shuttingDown&&loaded&&dirty)save();
}
void RecordController::togglePlayback(){if(busy||recoveryBlocked)return;if(recordPending||engine.state()==core::RecordAudioEngine::playing)stop();else preparePlayback(true);}
void RecordController::preparePlayback(bool start){
 if(start&&(!tone.available||!tone.available()||!tone.deviceRate||tone.deviceRate()<=0)){setMessage("Open Audio Settings and select an output before playback.");return;}
 if(busy||recordPending||recoveryBlocked)return;if(start&&pendingToneRestore){track(session,session.editableTrack>=0?session.editableTrack:session.activeTrack).tone=captureTone();pendingToneRestore=false;}
 engine.stop();cancelReference();session.cursor=engine.position();if(start)view.timeline.resumeFollowing();const auto copy=session;busy=true;playAfter=start;setMessage(start?"Preparing playback...":"Updating timeline...");job=std::async(std::launch::async,[this,copy]{Job j;j.session=copy;j.publish=true;j.result=loadSessionAudio(store.folder,copy,j.audio);return j;});
}
void RecordController::removeReference(){if(busy||recordPending)return;cancelReference();engine.setReferenceEnd(0);view.youtube.clear();view.youtubeURL.clear();session.videoId.clear();session.videoURL.clear();session.referenceStart=0;session.youtubeStart=0;dirty=true;update();setMessage("YouTube reference removed. Your guitar and local backing are unchanged.");}
void RecordController::seek(int64 p){if(busy||recordPending)return;const bool resume=referenceWanted||engine.state()==core::RecordAudioEngine::playing;engine.seek(p);cancelReference();session.cursor=p;dirty=true;view.timeline.revealPosition(p);syncReference(false);if(resume)preparePlayback(true);}
void RecordController::remove(int64 a,int64 b){if(busy||recordPending||engine.state()!=core::RecordAudioEngine::stopped)return;const int i=view.timeline.selectedTrack;if(i!=session.editableTrack){setMessage("Unfreeze Guitar "+String(i+1)+" before editing. Its rendered sound is preserved.");return;}core::eraseInterval(track(session,i).clips,a,b);dirty=true;update();preparePlayback(false);setMessage("Selection deleted from Guitar "+String(i+1)+". Later audio stays in place; original takes are intact.");}
void RecordController::selectTrack(int index,bool unfreeze){
 if(index<0||index>1||busy||recordPending||recoveryBlocked||engine.state()!=core::RecordAudioEngine::stopped)return;
 if(session.editableTrack==index){session.activeTrack=index;update();return;}
 if(session.editableTrack>=0&&!track(session,session.editableTrack).clips.empty()){
  if(switchDialog)return;const int previous=session.editableTrack;
  const auto message=l10n::format("record.switch_message",{{"first",String(previous+1)},{"second",String(index+1)}});
  auto* prompt=new popup::Prompt(message,"Render and switch");
  switchDialog=popup::launch(l10n::format("record.switch_title",{{"number",String(previous+1)}}),*prompt,view,true,0,ModalCallbackFunction::create([safe=Component::SafePointer<RecordView>(&view),this,previous,index,unfreeze](int answer){if(!safe)return;switchDialog=nullptr;if(!shuttingDown&&answer==1)renderTrack(previous,index,unfreeze);}));return;
 }
 captureEditable();auto& target=track(session,index);
 if(target.rendered.isNotEmpty()&&!unfreeze){session.activeTrack=index;session.editableTrack=-1;pendingToneRestore=false;dirty=true;update();preparePlayback(false);setMessage("Guitar "+String(index+1)+" is selected and keeps its sound. Click Unfreeze to change its preset.");return;}
 const auto snapshot=target.tone.isNotEmpty()?target.tone:captureTone();
 if(snapshot.isNotEmpty()&&tone.restore){auto result=tone.restore(snapshot);if(result.failed()){setMessage("Could not unfreeze this guitar: "+result.getErrorMessage());return;}}
 target.tone=snapshot;target.rendered.clear();session.activeTrack=session.editableTrack=index;pendingToneRestore=false;dirty=true;update();preparePlayback(false);setMessage("Guitar "+String(index+1)+" is editable. Preset changes affect this guitar only.");
}
void RecordController::renderTrack(int index,int switchAfter,bool unfreezeAfter){
 if(busy||recordPending||recoveryBlocked||index!=session.editableTrack||track(session,index).clips.empty()||!tone.createRenderer)return;
 engine.stop();cancelReference();session.cursor=engine.position();pendingToneRestore=false;captureEditable();auto copy=session;const auto snapshot=track(copy,index).tone;
 if(snapshot.isEmpty()){setMessage("Load a ToneGrid tone before rendering.");return;}String error;setMessage("Preparing Guitar "+String(index+1)+" for Render...");renderer=tone.createRenderer(snapshot,copy.rate,error);if(!renderer){setMessage(error);return;}
 busy=true;cancel=false;progress=0;auto* processor=renderer.get();
 job=std::async(std::launch::async,[this,copy,index,switchAfter,unfreezeAfter,processor]()mutable {Job j;std::unique_ptr<core::TimelineAudio> audio;auto& t=track(copy,index);j.result=core::TimelineAudio::load(store.folder,copy.rate,t.clips,"",audio,t.guitarLength);
  const auto name="guitar-"+String(index+1)+"-render-"+Uuid().toString()+".wav";
  if(j.result.wasOk())j.result=renderWave(*audio,*processor,store.folder.getChildFile(name),false,1,0,false,true,cancel,progress);
  if(j.result.wasOk()&&!cancel){t.rendered=name;copy.editableTrack=-1;j.result=loadSessionAudio(store.folder,copy,j.audio);if(j.result.wasOk())j.result=store.save(copy);if(j.result.wasOk()){j.session=copy;j.publish=true;j.switchAfter=switchAfter;j.unfreezeAfter=unfreezeAfter;j.message="Guitar "+String(index+1)+" is Rendered. Preset changes will not affect it. Unfreeze to change its sound.";}}return j;
 });
}
void RecordController::startRecording(){
 if(busy||recordPending||recoveryBlocked)return;if(session.editableTrack!=session.activeTrack){setMessage("Unfreeze a guitar using its Rendered/Unfreeze controls before recording.");return;}
 if(!tone.available||!tone.available()||!tone.deviceRate||tone.deviceRate()<=0){setMessage("Open Audio Settings and select your guitar input first.");return;}
 pendingToneRestore=false;captureEditable();if(track(session,session.activeTrack).tone.isEmpty()){setMessage("Load a ToneGrid tone before recording.");return;}
 engine.stop();cancelReference();engine.setSyncHeld(session.videoId.isNotEmpty());recordingStart=engine.position();recordingRate=tone.deviceRate();if(session.guitarLength==0&&session.second.guitarLength==0&&session.backing.isEmpty()){recordingStart=std::llround(recordingStart*recordingRate/session.rate);session.rate=recordingRate;}
 session.cursor=recordingStart;session.pendingTrack=session.activeTrack;session.pendingTake="guitar-"+String(session.activeTrack+1)+"-take-"+Uuid().toString()+".wav";session.pendingStart=recordingStart;session.pendingRate=recordingRate;
 engine.setPreviewSink(view.timeline.beginTake(session));const auto copy=session;const auto generation=engine.deviceGeneration();busy=true;recordPending=true;engine.seek(recordingStart);cancel=false;
 job=std::async(std::launch::async,[this,copy,generation]{Job j;j.result=store.save(copy);if(j.result.wasOk())j.result=engine.record(store.folder.getChildFile(copy.pendingTake),recordingRate,copy.rate,generation);j.recordArmed=j.result.wasOk();j.message=j.result.wasOk()?"Recording dry Guitar "+String(copy.activeTrack+1)+" - monitoring your current ToneGrid tone":"Recording did not start.";return j;});
}
void RecordController::stop(){
 playAfter=false;referenceWanted=referenceAwaiting=referenceBuffering=false;referenceDeadline=0;view.youtube.pauseTransport();if(busy){engine.setSyncHeld(true);stopRequested=true;return;}engine.quiesce();engine.setSyncHeld(false);captureEditable();
 if(recordPending){const auto file=session.pendingTake;const auto start=recordingStart;const auto rate=recordingRate;auto copy=session;busy=true;setMessage("Finishing take...");
  job=std::async(std::launch::async,[this,file,start,rate,copy]()mutable {Job j;const auto frames=engine.stop();copy.cursor=engine.position();if(frames>0)commitTake(copy,{file,start,(juce::int64)std::floor(frames*copy.rate/rate),0});copy.pendingTake.clear();j.session=copy;j.publish=true;j.result=store.save(copy);if(j.result.wasOk())j.result=loadSessionAudio(store.folder,copy,j.audio);j.message=engine.failed.load()?"Recording stopped early. Its captured portion and original takes are preserved.":"Take saved. Tweak your preset, then Render to keep this sound before recording the other guitar.";return j;});recordPending=false;
 }else{engine.stop();session.cursor=engine.position();dirty=true;captureEditable();save();}
}
void RecordController::loadBacking(){if(busy||recordPending||chooser)return;chooser=std::make_unique<FileChooser>("Load backing audio",File{},"*.mp3;*.wav;*.flac;*.aiff;*.aif");chooser->launchAsync(FileBrowserComponent::openMode|FileBrowserComponent::canSelectFiles,[this](const FileChooser& c){const auto original=c.getResult();chooser.reset();if(original!=File{})importBacking(original);});}
void RecordController::importBacking(const File& original){
 if(busy||recordPending||recoveryBlocked||original==File{})return;engine.stop();busy=true;auto copy=session;
 job=std::async(std::launch::async,[this,copy,original]()mutable {Job j;const auto name="backing-"+Uuid().toString()+original.getFileExtension();const auto target=store.folder.getChildFile(name);if(store.folder.createDirectory().failed()||!original.copyFileTo(target)){j.result=Result::fail("Cannot copy the local backing track. Original file was not changed.");return j;}copy.backing=name;j.result=loadSessionAudio(store.folder,copy,j.audio);if(j.result.wasOk()){j.session=copy;j.publish=true;j.message="Local backing track loaded at 00:00.";}return j;});
}
void RecordController::chooseExport(bool mix,bool selection){
 if(busy||recordPending||chooser)return;if(selection&&!view.timeline.hasSelection()){setMessage("Drag across the timeline to select a time range first.");return;}
 if(session.guitarLength<=0&&session.second.guitarLength<=0&&session.backing.isEmpty()){setMessage(session.videoId.isNotEmpty()?"YouTube policy: reference audio is excluded. Record guitar or load local backing before exporting.":"Record guitar or load local backing first.");return;}
 chooser=std::make_unique<FileChooser>(selection?"Export time selection MP3":"Export all MP3",store.folder.getChildFile(selection?"ToneGrid Selection.mp3":"ToneGrid All.mp3"),"*.mp3");chooser->launchAsync(FileBrowserComponent::saveMode|FileBrowserComponent::canSelectFiles|FileBrowserComponent::warnAboutOverwriting,[this,mix,selection](const FileChooser& c){const auto target=c.getResult();chooser.reset();if(target!=File{})exportAudio(target.withFileExtension("mp3"),mix,selection);});
}
void RecordController::exportAudio(File target,bool mix,bool selection){
 if(busy||recordPending||!tone.createRenderer)return;if(session.guitarLength<=0&&session.second.guitarLength<=0&&session.backing.isEmpty()&&session.videoId.isNotEmpty()){setMessage("YouTube policy: reference audio is excluded. Record guitar or load local backing before exporting.");return;}
 if(selection&&!view.timeline.hasSelection()){setMessage("Drag across the timeline to select a time range first.");return;}
 engine.stop();cancelReference();pendingToneRestore=false;captureEditable();session.cursor=engine.position();String error;setMessage("Preparing MP3 export...");
 const bool needsTone=session.editableTrack>=0&&!track(session,session.editableTrack).clips.empty();
 if(needsTone){const auto snapshot=track(session,session.editableTrack).tone;if(snapshot.isEmpty()){setMessage("No current ToneGrid tone is available.");return;}renderer=tone.createRenderer(snapshot,session.rate,error);}else renderer=std::make_unique<SilentRenderer>();
 if(!renderer){setMessage(error);return;}busy=true;cancel=false;progress=0;const auto copy=session;auto* processor=renderer.get();const auto from=selection?view.timeline.selectionFrom():0,to=selection?view.timeline.selectionTo():0;
 job=std::async(std::launch::async,[this,copy,target,mix,processor,from,to]{Job j;std::unique_ptr<core::TimelineAudio> audio;j.result=loadSessionAudio(store.folder,copy,audio);const auto& t=track(copy,copy.editableTrack>=0?copy.editableTrack:copy.activeTrack);if(j.result.wasOk()){core::LameMp3Encoder enc(encoder);j.result=renderExport(*audio,*processor,enc,target,mix,t.guitarVolume,copy.backingVolume,copy.editableTrack<0||t.guitarMute,copy.backingMute,cancel,progress,from,to,{copy.guitarMute?0:copy.guitarVolume,copy.second.guitarMute?0:copy.second.guitarVolume});}j.message="MP3 exported: "+target.getFileName();return j;});
}
void RecordController::save(){if(busy||!loaded||recoveryBlocked||recordPending)return;captureEditable();session.cursor=engine.position();const auto copy=session;dirty=false;busy=true;job=std::async(std::launch::async,[this,copy]{Job j;j.result=store.save(copy);return j;});}
void RecordController::timerCallback(){
 finishJob();if(shuttingDown)return;if(tone.processingLatency)engine.setPlaybackLatency(tone.processingLatency());
 if(active&&session.videoId.isNotEmpty()){
  const bool visible=view.referenceVisible();
  if(!visible&&referenceVisibleLast){if(referenceWanted||recordPending)referenceInterrupted("Scroll to the YouTube reference to continue.");else view.youtube.pauseTransport();}
  if(visible)ensureReference();referenceVisibleLast=visible;
 }
 if(referenceWanted&&referenceDeadline>0&&Time::getMillisecondCounterHiRes()>referenceDeadline)referenceInterrupted("YouTube did not become ready. Click Play in the video or reload it.");
 if(!active&&!busy&&recordPending)stop();if(!busy&&recordPending&&(engine.failed.load()||engine.deviceLost.load())){stop();setMessage("Recording stopped after an audio-device or disk-buffer interruption. The captured audio is being preserved.");}
 const bool playing=engine.state()==core::RecordAudioEngine::playing;if(wasPlaying&&engine.state()==core::RecordAudioEngine::stopped&&!recordPending&&!referenceAwaiting)cancelReference();wasPlaying=playing;
 view.setBusy(busy,recordPending,playing);const auto position=engine.position();view.time.setText(TimelineView::timeText(position/session.rate),dontSendNotification);view.timeline.setPosition(position,engine.state()!=core::RecordAudioEngine::stopped);
 if(renderer)setMessage("Rendering audio... "+String(int(progress.load()*100))+"% - Stop cancels safely");
 if(active&&++autosaveTicks>=150){autosaveTicks=0;if(!busy&&!recordPending&&engine.state()==core::RecordAudioEngine::stopped){dirty=true;save();}}
 if(!active&&!busy&&!recordPending){if(dirty)save();else stopTimer();}
}
#include "Modules/Record/RecordReferenceSync.inl"
}
