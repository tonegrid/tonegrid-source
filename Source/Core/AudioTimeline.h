#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <map>
namespace tonegrid::core {
struct Segment {
 juce::String file;juce::int64 start=0,length=0;double sourceStart=0;
};
inline bool safeRelative(const juce::String& s) {
 if(s.isEmpty()||s.length()>512||s.startsWithChar('/')||s.containsAnyOf("\\:\r\n"))return false;
 for(auto& p:juce::StringArray::fromTokens(s,"/",{}))if(p.isEmpty()||p==".."||p==".")return false;
 return true;
}
inline void eraseInterval(std::vector<Segment>& clips,juce::int64 from,juce::int64 to) {
 if(from<0||to<=from)return;std::vector<Segment> result;
 for(const auto& c:clips) {
  const auto end=c.start+c.length;if(end<=from||c.start>=to){result.push_back(c);continue;}
  if(c.start<from){auto left=c;left.length=from-c.start;result.push_back(left);}
  if(end>to){auto right=c;right.sourceStart+=double(to-c.start);right.start=to;right.length=end-to;result.push_back(right);}
 }
 clips=std::move(result);
}
inline void punch(std::vector<Segment>& clips,Segment c) {
 if(c.length<=0)return;eraseInterval(clips,c.start,c.start+c.length);clips.push_back(std::move(c));
 std::sort(clips.begin(),clips.end(),[](const auto& a,const auto& b){return a.start<b.start;});
}
struct AudioFile {juce::AudioBuffer<float> samples;double rate=48000;};
struct RenderedLane {juce::String file;int track=0;const AudioFile* prepared=nullptr;};
struct TimelineAudio {
 double rate=48000;std::vector<Segment> clips;juce::String backing;juce::File sourceFolder;
 std::map<juce::String,std::unique_ptr<AudioFile>> files;
 juce::int64 guitarEnd=0,backingEnd=0;
 std::vector<RenderedLane> rendered;
 const AudioFile* preparedBacking=nullptr;std::vector<const AudioFile*> preparedClips;
 float sample(const juce::String& file,double frame,int channel)const noexcept {
  auto it=files.find(file);return it==files.end()?0:sample(it->second.get(),frame,channel);
 }
 float sample(const AudioFile* prepared,double frame,int channel)const noexcept {
  if(!prepared)return 0;const auto& f=*prepared;
  const auto sourceFrame=frame*f.rate/rate;const auto index=(juce::int64)std::floor(sourceFrame);
  if(index<0||index>=f.samples.getNumSamples())return 0;
  const auto c=juce::jmin(channel,f.samples.getNumChannels()-1);const float a=f.samples.getSample(c,(int)index);
  const float b=index+1<f.samples.getNumSamples()?f.samples.getSample(c,(int)index+1):0;
  const float v=a+(b-a)*float(sourceFrame-index);return std::isfinite(v)?v:0;
 }
 float sampleBacking(double frame,int channel)const noexcept {
  return preparedBacking?sample(preparedBacking,frame,channel):sample(backing,frame,channel);
 }
 float sample(const RenderedLane& lane,double frame,int channel)const noexcept {
  return lane.prepared?sample(lane.prepared,frame,channel):sample(lane.file,frame,channel);
 }
 float guitar(double frame)const noexcept {
  auto next=std::upper_bound(clips.begin(),clips.end(),frame,[](double at,const Segment& clip){return at<clip.start;});
  if(next==clips.begin())return 0;const auto& c=*std::prev(next);
  if(frame>=c.start&&frame<c.start+c.length) {
   // Unexposed 1 ms edge ramps prevent discontinuities after delete/punch.
   const auto edge=juce::jmin(rate*.001,double(c.length)/2);
   const float gain=float(juce::jlimit(0.,1.,juce::jmin((frame-c.start)/edge,(c.start+c.length-frame)/edge)));
   const auto index=size_t(std::distance(clips.begin(),std::prev(next)));
   return (index<preparedClips.size()?sample(preparedClips[index],c.sourceStart+frame-c.start,0):sample(c.file,c.sourceStart+frame-c.start,0))*gain;
  }return 0;
 }
 static juce::Result load(const juce::File& folder,double rate,const std::vector<Segment>& clips,const juce::String& backing,std::unique_ptr<TimelineAudio>& out,juce::int64 extent=0,const std::vector<RenderedLane>& rendered={}) {
  if(!std::isfinite(rate)||rate<8000||rate>192000)return juce::Result::fail("Invalid session sample rate.");
  auto result=std::make_unique<TimelineAudio>();result->sourceFolder=folder;result->guitarEnd=extent;result->rate=rate;result->clips=clips;result->backing=backing;
  result->rendered=rendered;
  juce::StringArray names;for(const auto& c:clips){names.addIfNotAlreadyThere(c.file);result->guitarEnd=juce::jmax(result->guitarEnd,c.start+c.length);}if(backing.isNotEmpty())names.addIfNotAlreadyThere(backing);for(const auto& lane:rendered)names.addIfNotAlreadyThere(lane.file);
  juce::AudioFormatManager formats;formats.registerBasicFormats();juce::int64 bytes=0;
  for(const auto& name:names) {
   if(!safeRelative(name))return juce::Result::fail("Invalid recording path.");const auto file=folder.getChildFile(name);
   if(file.isSymbolicLink())return juce::Result::fail("Linked recording files are not supported.");
   std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
   if(!reader||reader->numChannels<1||reader->lengthInSamples<=0||reader->sampleRate<8000||reader->sampleRate>192000)return juce::Result::fail("Cannot read session audio: "+name);
   const int channels=juce::jmin(2,(int)reader->numChannels);bytes+=reader->lengthInSamples*channels*4;
   if(bytes>512ll*1024*1024||reader->lengthInSamples>std::numeric_limits<int>::max())return juce::Result::fail("This session exceeds the 512 MB playback cache. Source recordings are intact.");
   auto f=std::make_unique<AudioFile>();f->rate=reader->sampleRate;f->samples.setSize(channels,(int)reader->lengthInSamples);
   if(!reader->read(&f->samples,0,f->samples.getNumSamples(),0,true,channels>1))return juce::Result::fail("Cannot decode session audio: "+name);
   if(name==backing)result->backingEnd=std::llround(reader->lengthInSamples*rate/reader->sampleRate);
   result->files.emplace(name,std::move(f));
  }
  const auto resolve=[&](const juce::String& name)->const AudioFile*{auto it=result->files.find(name);return it==result->files.end()?nullptr:it->second.get();};
  result->preparedBacking=resolve(backing);
  for(const auto& c:clips)result->preparedClips.push_back(resolve(c.file));
  for(auto& lane:result->rendered)lane.prepared=resolve(lane.file);
  out=std::move(result);return juce::Result::ok();
 }
};
}
