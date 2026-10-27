#pragma once
#include "Core/AudioTimeline.h"
#include <json.hpp>
namespace tonegrid::record {
struct GuitarTrack {
 std::vector<core::Segment> clips;juce::String tone,rendered;
 juce::int64 guitarLength=0;float guitarVolume=1;bool guitarMute=false;
};
// Inherited first-track fields preserve compatibility with schema-1 recordings.
struct RecordSession:GuitarTrack {
 GuitarTrack second;int activeTrack=0,editableTrack=0,pendingTrack=0;
 double rate=48000;juce::String backing,videoId,videoURL;
 juce::int64 cursor=0,referenceStart=0;double youtubeStart=0;
 juce::String pendingTake;juce::int64 pendingStart=0;double pendingRate=48000;
 float backingVolume=.7f;bool backingMute=false;
};
inline GuitarTrack& track(RecordSession& s,int i){return i==1?s.second:static_cast<GuitarTrack&>(s);}
inline const GuitarTrack& track(const RecordSession& s,int i){return i==1?s.second:static_cast<const GuitarTrack&>(s);}
inline void commitTake(RecordSession& s,core::Segment take){auto& t=track(s,s.pendingTrack);t.guitarLength=juce::jmax(t.guitarLength,take.start+take.length);t.rendered.clear();core::punch(t.clips,std::move(take));}
inline juce::Result loadSessionAudio(const juce::File& folder,const RecordSession& s,std::unique_ptr<core::TimelineAudio>& out){
 std::vector<core::RenderedLane> frozen;for(int i=0;i<2;++i)if(track(s,i).rendered.isNotEmpty())frozen.push_back({track(s,i).rendered,i});
 return core::TimelineAudio::load(folder,s.rate,s.editableTrack>=0?track(s,s.editableTrack).clips:std::vector<core::Segment>{},s.backing,out,juce::jmax(s.guitarLength,s.second.guitarLength),frozen);
}
class RecordSessionStore {
 using J=nlohmann::json;
 static J encode(const GuitarTrack& s){
  J clips=J::array();for(const auto& c:s.clips)clips.push_back({{"file",c.file.toStdString()},{"start",c.start},{"length",c.length},{"sourceStart",c.sourceStart}});
  auto extent=s.guitarLength;for(const auto& c:s.clips)extent=juce::jmax(extent,c.start+c.length);
  return {{"clips",clips},{"toneSnapshot",s.tone.toStdString()},{"guitarLength",extent},{"guitarVolume",s.guitarVolume},{"guitarMute",s.guitarMute},{"rendered",s.rendered.toStdString()}};
 }
 static GuitarTrack decode(const J& j){
  GuitarTrack s;s.tone=juce::String(j.at("toneSnapshot").get<std::string>());s.guitarVolume=j.at("guitarVolume");s.guitarMute=j.at("guitarMute");s.rendered=juce::String(j.value("rendered",std::string{}));
  if(!std::isfinite(s.guitarVolume)||s.guitarVolume<0||s.guitarVolume>2||(s.rendered.isNotEmpty()&&!core::safeRelative(s.rendered)))throw std::runtime_error("Invalid guitar track.");
  if(j.at("clips").size()>100000)throw std::runtime_error("Too many recording segments.");
  juce::int64 last=0;for(const auto& c:j.at("clips")){
   core::Segment seg{juce::String(c.at("file").get<std::string>()),c.at("start"),c.at("length"),c.at("sourceStart")};
   if(!core::safeRelative(seg.file)||seg.start<last||seg.length<=0||seg.length>192000ll*86400||seg.start>192000ll*86400-seg.length||!std::isfinite(seg.sourceStart)||seg.sourceStart<0||seg.sourceStart>192000.*86400)throw std::runtime_error("Invalid recording segment.");last=seg.start+seg.length;s.clips.push_back(seg);
  }
  s.guitarLength=j.value("guitarLength",last);if(s.guitarLength<last||s.guitarLength>192000ll*86400)throw std::runtime_error("Invalid Guitar timeline length.");return s;
 }
public:
 explicit RecordSessionStore(juce::File directory):folder(std::move(directory)){}
 const juce::File folder;
 juce::Result save(const RecordSession& s)const {
  J j=encode(s);j["schemaVersion"]=2;j["secondGuitar"]=encode(s.second);j["activeTrack"]=s.activeTrack;j["editableTrack"]=s.editableTrack;
  j["rate"]=s.rate;j["cursor"]=s.cursor;j["backing"]=s.backing.toStdString();j["backingVolume"]=s.backingVolume;j["backingMute"]=s.backingMute;
  j["youtube"]={{"videoId",s.videoId.toStdString()},{"url",s.videoURL.toStdString()},{"recordTimelineStart",s.referenceStart},{"youtubeStartSeconds",s.youtubeStart}};
  j["pendingTake"]={{"file",s.pendingTake.toStdString()},{"start",s.pendingStart},{"rate",s.pendingRate},{"track",s.pendingTrack}};
  if(folder.isSymbolicLink()||folder.createDirectory().failed())return juce::Result::fail("Cannot create the recordings folder.");
  const auto target=folder.getChildFile("session.json");juce::TemporaryFile temp(target);
  if(!temp.getFile().replaceWithText(juce::String(j.dump(2)))||!temp.overwriteTargetFileWithTemporary())return juce::Result::fail("Could not autosave. Your source recordings are intact.");return juce::Result::ok();
 }
 juce::Result load(RecordSession& out)const {
  const auto file=folder.getChildFile("session.json");if(!file.existsAsFile())return juce::Result::ok();
  if(file.isSymbolicLink()||file.getSize()>96*1024*1024)return juce::Result::fail("Invalid Record session. Source files were preserved.");
  try {
   const auto j=J::parse(file.loadFileAsString().toStdString());const int version=j.at("schemaVersion");if(version!=1&&version!=2)throw std::runtime_error("Session version is unsupported.");RecordSession s;static_cast<GuitarTrack&>(s)=decode(j);
   if(version==2){s.second=decode(j.at("secondGuitar"));s.activeTrack=j.at("activeTrack");s.editableTrack=j.at("editableTrack");}
   if(s.activeTrack<0||s.activeTrack>1||s.editableTrack< -1||s.editableTrack>1)throw std::runtime_error("Invalid active guitar.");
   for(int i=0;i<2;++i){const auto& t=track(s,i);if((i==s.editableTrack&&t.rendered.isNotEmpty())||(i!=s.editableTrack&&!t.clips.empty()&&t.rendered.isEmpty()))throw std::runtime_error("Invalid rendered/editable track state.");}
   s.rate=j.at("rate");s.cursor=j.at("cursor");s.backing=juce::String(j.at("backing").get<std::string>());
   if(!std::isfinite(s.rate)||s.rate<8000||s.rate>192000||s.cursor<0||s.cursor>192000ll*86400||(s.backing.isNotEmpty()&&!core::safeRelative(s.backing)))throw std::runtime_error("Invalid session timeline.");
   s.backingVolume=j.at("backingVolume");s.backingMute=j.at("backingMute");if(!std::isfinite(s.backingVolume)||s.backingVolume<0||s.backingVolume>2)throw std::runtime_error("Invalid backing level.");
   if(j.contains("youtube")){const auto& y=j.at("youtube");s.videoId=juce::String(y.value("videoId",std::string{}));s.videoURL=juce::String(y.value("url",std::string{}));s.referenceStart=y.value("recordTimelineStart",juce::int64{});s.youtubeStart=y.value("youtubeStartSeconds",0.);if(s.referenceStart<0||!std::isfinite(s.youtubeStart)||s.youtubeStart<0)throw std::runtime_error("Invalid reference offset.");}
   if(j.contains("pendingTake")){const auto& p=j.at("pendingTake");s.pendingTake=juce::String(p.value("file",std::string{}));s.pendingStart=p.value("start",juce::int64{});s.pendingRate=p.value("rate",48000.);s.pendingTrack=p.value("track",0);if((s.pendingTake.isNotEmpty()&&!core::safeRelative(s.pendingTake))||s.pendingStart<0||s.pendingTrack<0||s.pendingTrack>1||(s.pendingTake.isNotEmpty()&&s.pendingTrack!=s.activeTrack)||!std::isfinite(s.pendingRate)||s.pendingRate<8000||s.pendingRate>192000)throw std::runtime_error("Invalid pending take.");}
   out=std::move(s);return juce::Result::ok();
  }catch(const std::exception& e){return juce::Result::fail("Cannot recover Record session: "+juce::String(e.what())+" Source files were preserved.");}
 }
};
}