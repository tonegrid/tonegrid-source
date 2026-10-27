#pragma once
#include <juce_core/juce_core.h>
namespace tonegrid::record {
inline juce::String youtubeBridgeScript(){return R"TGJS(
var player,ready=false;
(function(){
 'use strict';
 var request={id:-1,time:0,play:false},starting=false,stopping=false,last=null,sequence=0,guard=0,mutedForSeek=false,restoreMuted=true,error=0,blocked=false;
 var now=function(){return performance.now();};
 function restoreAudio(){if(mutedForSeek){mutedForSeek=false;if(!restoreMuted)player.unMute();}}
 function report(reason){
  if(!ready)return;
  var time=Number(player.getCurrentTime())||0,state=player.getPlayerState(),rate=Number(player.getPlaybackRate())||1;
  var sample={command:request.id,sequence:++sequence,time:time,state:state,rate:rate,duration:Number(player.getDuration())||0,ready:true,starting:starting,blocked:blocked,error:error,reason:reason||'sample'};
  if(window.__JUCE__&&window.__JUCE__.backend)window.__JUCE__.backend.emitEvent('recordVideoStatus',sample);
  return sample;
 }
 function tick(reason){
  if(!ready)return;
  var t=Number(player.getCurrentTime())||0,state=player.getPlayerState(),stamp=now(),rate=Number(player.getPlaybackRate())||1;
  if(state===1)blocked=false;
  if(stopping&&state===1&&stamp<guard){player.pauseVideo();report('command');return;}
  if(starting&&reason!=='command'&&state===1&&t>=request.time-0.025&&t<=request.time+0.35){
   starting=false;guard=stamp+300;
   if(!request.play){stopping=true;guard=stamp+500;player.pauseVideo();}
   restoreAudio();last={time:t,state:request.play?1:2,rate:rate,stamp:stamp};report(request.play?'started':'positioned');return;
  }
  var jumped=false;
  if(!starting&&last&&stamp>guard){
   var expected=last.time+(last.state===1?(stamp-last.stamp)*last.rate/1000:0);
   jumped=Math.abs(t-expected)>0.35;
  }
  last={time:t,state:state,rate:rate,stamp:stamp};report(jumped?'seek':reason);
 }
 function apply(){
  if(!ready)return;
  blocked=false;starting=true;stopping=false;last=null;guard=now()+500;
  if(error){starting=false;report('error');return;}
  restoreMuted=player.isMuted();if(!restoreMuted){player.mute();mutedForSeek=true;}
  player.seekTo(request.time,true);
  // Await actual media time. Cold seeks may land on a preceding keyframe.
  // Muted pre-roll reaches the requested frame before paused positioning ends.
  player.playVideo();tick('command');
 }
 window.tgRecord={
  command:function(id,time,play){if(id<=request.id)return;restoreAudio();request={id:id,time:Math.max(0,Number(time)||0),play:!!play};apply();},
  pause:function(id){if(id<=request.id)return;request={id:id,time:ready?(Number(player.getCurrentTime())||0):0,play:false};starting=false;stopping=true;guard=now()+300;last=null;if(ready){player.pauseVideo();restoreAudio();tick('command');}},
  inspect:function(){return {ready:ready,command:request.id,starting:starting,time:ready?player.getCurrentTime():0,state:ready?player.getPlayerState():-1,error:error,blocked:blocked,width:innerWidth,height:innerHeight,videoId:ready&&player.getVideoData?player.getVideoData().video_id:''};}
 };
 document.addEventListener('fullscreenchange',function(){if(window.__JUCE__)window.__JUCE__.backend.emitEvent('recordVideoFullscreen',!!document.fullscreenElement);});
 window.onYouTubeIframeAPIReady=function(){
  player=new YT.Player('yt',{events:{
   onReady:function(){ready=true;report('ready');if(request.id>=0)apply();},
   onStateChange:function(){tick('state');},
   onPlaybackRateChange:function(){tick('rate');},
   onAutoplayBlocked:function(){blocked=true;report('blocked');},
   onError:function(e){error=Number(e.data)||1;report('error');}
  }});
 };
 window.setInterval(function(){tick('sample');},20);
 var tag=document.createElement('script');tag.src='https://www.youtube.com/iframe_api';document.head.appendChild(tag);
})();
)TGJS";}
struct VideoStatus {
 int command=0,sequence=0,state=-1,error=0;double seconds=0,duration=0,rate=1;
 bool starting=false,blocked=false;juce::String reason;
};
class VideoDriftGuard {
public:
 bool observe(double error,double now){if(std::abs(error)>.15)++outside;else outside=0;if(outside<3||now-lastCorrection<2000)return false;outside=0;lastCorrection=now;return true;}
 void reset(){outside=0;lastCorrection=-10000;}
private:int outside=0;double lastCorrection=-10000;
};
}
