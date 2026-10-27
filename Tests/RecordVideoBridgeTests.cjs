const fs=require('fs'),vm=require('vm'),assert=require('assert');
const header=fs.readFileSync(require('path').join(__dirname,'../Source/Modules/Record/YouTube/YouTubeBridgeScript.h'),'utf8');
const code=header.split('R"TGJS(')[1].split(')TGJS"')[0];let checks=0;
function check(v,s){assert(v,s);checks++;process.stdout.write('PASS '+s+'\n');}
function fixture(){
 let clock=0,tick,events=[],full,player;const ctx={console,performance:{now:()=>clock},innerWidth:800,innerHeight:450};ctx.window=ctx;
 ctx.setInterval=f=>tick=f;ctx.__JUCE__={backend:{emitEvent:(type,value)=>events.push({type,...(typeof value==='object'?value:{active:value})})}};
 ctx.document={addEventListener:(type,f)=>{full=f;},createElement:()=>({}),head:{appendChild:()=>{}}};
 ctx.YT={Player:function(id,o){player=this;this.t=0;this.state=2;this.rate=1;this.muted=false;this.cold=false;this.ev=o.events;this.seekTo=(x)=>{this.t=this.cold?Math.floor(x/3)*3:x;};this.playVideo=()=>{this.state=1;};this.pauseVideo=()=>{this.state=2;};this.getCurrentTime=()=>this.t;this.getPlayerState=()=>this.state;this.getPlaybackRate=()=>this.rate;this.getDuration=()=>180;this.getVideoData=()=>({video_id:'M7lc1UVf-VE'});this.isMuted=()=>this.muted;this.mute=()=>{this.muted=true;};this.unMute=()=>{this.muted=false;};}};
 vm.createContext(ctx);vm.runInContext(code,ctx);return {ctx,events,ready(){ctx.onYouTubeIframeAPIReady();player.ev.onReady();return player;},step(ms=40){clock+=ms;if(player.state===1)player.t+=ms*player.rate/1000;tick();},get player(){return player;},full(){ctx.document.fullscreenElement={};full();}};
}
{
 const f=fixture();f.ctx.tgRecord.command(7,12.651,true);const p=f.ready();p.cold=true;f.ctx.tgRecord.command(8,12.651,true);
 check(f.ctx.tgRecord.inspect().starting&&p.muted,'A cold fractional seek waits in muted pre-roll instead of acknowledging a preceding keyframe');
 for(let i=0;i<10;i++)f.step();check(!f.events.some(x=>x.reason==='started'&&x.command===8),'Audio cannot be released before the requested media position');
 for(let i=0;i<55;i++)f.step();const start=f.events.find(x=>x.reason==='started'&&x.command===8);
 check(start&&Math.abs(start.time-12.651)<.05&&!p.muted,'Playback starts from observed fractional media time within one 40 ms poll');
 f.ctx.tgRecord.command(7,0,false);check(f.ctx.tgRecord.inspect().command===8,'A delayed older command cannot overwrite the newest transport request');
 f.ctx.tgRecord.pause(9);p.state=1;p.ev.onStateChange();check(p.state===2,'A delayed PLAYING echo cannot resurrect a stopped transport');
 f.step(400);p.state=1;p.ev.onStateChange();check(f.events.at(-1).state===1,'A later direct click in the video is still observable');
 p.state=2;p.ev.onStateChange();check(f.events.at(-1).state===2,'Direct pause on the YouTube controls is reported to the native transport');
 p.t=50;p.ev.onStateChange();check(f.events.at(-1).reason==='seek','A paused seek using YouTube controls is detected without a nonexistent seek event');
 p.state=3;p.ev.onStateChange();check(f.events.at(-1).state===3,'Buffering is reported instead of letting the audio clock run unattended');
 p.ev.onAutoplayBlocked();check(f.events.at(-1).blocked,'Autoplay blocking is reported so recording can remain safely armed');
 p.ev.onError({data:153});check(f.events.at(-1).error===153,'Embedding errors reach the transport instead of silently starting guitar playback');
 f.ctx.tgRecord.command(10,0,true);check(f.events.at(-1).error===153,'A later play command does not erase a fatal player error or falsely release the transport');
 f.full();check(f.events.at(-1).type==='recordVideoFullscreen'&&f.events.at(-1).active,'The player fullscreen event is forwarded to the native window host');
}
{
 const f=fixture();const p=f.ready();p.cold=true;f.ctx.tgRecord.command(1,12.651,false);
 for(let i=0;i<100;i++)f.step();const at=f.events.find(x=>x.reason==='positioned');
 check(at&&Math.abs(at.time-12.651)<.05&&p.state===2&&!p.muted,'Paused fractional positioning reaches its requested frame and restores the prior mute setting');
 const before=f.events.length;f.ctx.tgRecord.command(1,80,true);f.step();check(f.ctx.tgRecord.inspect().command===1&&p.state===2&&f.events.length>before,'Repeated page-ready delivery of the same command is idempotent');
 p.rate=.5;p.ev.onPlaybackRateChange();check(f.events.at(-1).rate===.5,'Unsupported video speed is observable and can be rejected without drift');
}
{
 const f=fixture();const p=f.ready();p.t=55;p.state=1;
 const seek=p.seekTo;p.seekTo=()=>{};f.ctx.tgRecord.command(1,0,true);f.step(40);
 check(f.ctx.tgRecord.inspect().starting&&!f.events.some(x=>x.reason==='started'),'A cached PLAYING position from before a backward seek cannot release the audio clock');
 p.seekTo=seek;p.t=0;f.step(40);check(f.events.at(-1).reason==='started','The newest backward seek starts only after its actual target is observed');
 p.state=2;p.ev.onAutoplayBlocked();p.state=1;p.ev.onStateChange();
 check(!f.events.at(-1).blocked,'A trusted manual Play recovers from autoplay blocking and can release the waiting transport');
}
{
 const f=fixture();const p=f.ready();p.pauseVideo=()=>{};f.ctx.tgRecord.command(1,0,false);f.step(40);f.step(40);
 check(f.events.at(-1).reason==='command'&&f.events.at(-1).state===1,'Delayed PLAYING echoes while paused positioning settles cannot masquerade as a manual Play');
 p.state=2;f.step(600);p.state=1;p.ev.onStateChange();check(f.events.at(-1).reason!=='command','A later real Play remains available after paused positioning settles');
}
console.log(checks+' RECORD YouTube bridge checks; ALL PASSED');
