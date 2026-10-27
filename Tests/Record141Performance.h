#pragma once
#include "Core/RecordAudioEngine.h"
#include "Core/ToneService.h"
#include <juce_audio_utils/juce_audio_utils.h>
namespace record141Performance {
using namespace juce;using namespace tonegrid;
struct Preview:AudioFormatWriter::ThreadedWriter::IncomingDataReceiver {
 AudioThumbnail& thumbnail;std::atomic<int64> nanos{0};
 explicit Preview(AudioThumbnail& t):thumbnail(t){}
 void reset(int channels,double rate,int64 total)override{thumbnail.reset(channels,rate,total);}
 void addBlock(int64 frame,const AudioBuffer<float>& b,int start,int count)override{const auto now=Time::getHighResolutionTicks();thumbnail.addBlock(frame,b,start,count);nanos.fetch_add((Time::getHighResolutionTicks()-now)*1000000000ll/Time::getHighResolutionTicksPerSecond());}
};
inline bool run(const File& root,core::IRenderProcessor& processor,String& report) {
 AudioFormatManager formats;formats.registerBasicFormats();AudioThumbnailCache cache(2);AudioThumbnail thumbnail(512,formats,cache);Preview preview(thumbnail);
 core::RecordAudioEngine engine;engine.deviceStarted(48000);engine.setEnabled(true);AudioBuffer<float> raw(2,512),buffer(2,512);bool finite=true;int phase=0;
 for(int mode=0;mode<7;++mode) {
  engine.stop();std::unique_ptr<core::TimelineAudio> audio;
  const bool backing=mode==3||mode>=4,recording=mode==2||mode>=5,playing=mode==1||mode==3||mode==4;
  if(core::TimelineAudio::load(root,48000,mode==3?std::vector<core::Segment>{}:std::vector<core::Segment>{{"di.wav",0,20992,0}},backing?String("backing.wav"):String{},audio).failed())return false;
  engine.setTimeline(std::move(audio));engine.seek(0);engine.controls(1,.7f,mode==3,false);engine.setPreviewSink(mode==6?&preview:nullptr);
  if(recording&&engine.record(root.getChildFile("performance-"+String(mode)+".wav"),48000,48000).failed())return false;
  if(playing)engine.play();std::vector<double> micros;micros.reserve(200);
  for(int block=0;block<200;++block) {
   if(playing&&engine.state()==core::RecordAudioEngine::stopped){engine.seek(0);engine.play();}
   for(int i=0;i<512;++i){raw.setSample(0,i,.025f*std::sin(float(phase+i)*.037f));raw.setSample(1,i,0);}phase+=512;
   const auto start=Time::getMillisecondCounterHiRes();const auto* input=engine.input(raw.getArrayOfReadPointers(),2,512);
   buffer.copyFrom(0,0,input[0],512);buffer.copyFrom(1,0,input[1],512);processor.process(buffer);engine.output(buffer.getArrayOfWritePointers(),2,512);
   micros.push_back((Time::getMillisecondCounterHiRes()-start)*1000);
   for(int c=0;c<2;++c)for(int i=0;i<512;++i)finite&=std::isfinite(buffer.getSample(c,i));
   if(recording&&block%4==0)Thread::sleep(1);
  }
  engine.stop();double sum=0;for(const auto value:micros)sum+=value;std::sort(micros.begin(),micros.end());const double average=sum/micros.size();
  const StringArray names{"idle_live_monitor","DI_playback","DI_recording","backing_only","DI_and_backing_playback","live_recording_with_backing","recording_backing_and_live_waveform"};
  report+="MEASURE mode="+names[mode]+" mean_us_per_512="+String(average,3)+" p95_us="+String(micros[190],3)+" one_core_audio_budget_percent="+String(average/10666.6667*100,3)+"\n";
  finite&=!engine.failed.load();
 }
 report+="MEASURE background_thumbnail_total_ms="+String(preview.nanos.load()/1000000.,3)+"\n";
 return finite;
}
}
