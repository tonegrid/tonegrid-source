#pragma once
#include "UserPedalLibrary.h"
#include "LoopRecorder.h"
#include "Content/Presets/PresetShare.h"
#include "Effects/AmpEq/AmpEqProcessor.h"
#include "Effects/AmpEq/AdvancedEqControls.h"
#include "RealtimeAllocationProbe.h"
#include "Tone3000.h"
#include "Modules/Play/UI/PedalCard.h"
namespace release122Tests
{
inline bool run(const juce::File& models,juce::String& report)
{
    using namespace juce;using namespace tonegrid;bool ok=true;int checks=0;
    const auto check=[&](bool pass,const String& name){++checks;ok&=pass;report+=String(pass?"PASS ":"FAIL ")+"122 "+name+"\n";};
    const auto root=File::getSpecialLocation(File::tempDirectory).getChildFile("ToneGrid-release122-"+Uuid().toString());root.createDirectory();
    const ScopeGuard cleanup{[&]{if(root.isAChildOf(File::getSpecialLocation(File::tempDirectory)))root.deleteRecursively();}};
    auto payload=[](bool calibration=true){nlohmann::json j={{"version","0.7.0"},{"architecture","Linear"},{"sample_rate",48000},{"config",{{"receptive_field",1},{"bias",false}}},{"weights",{1.f}}};if(calibration)j["metadata"]={{"input_level_dbu",11.5},{"output_level_dbu",11.5}};return j;};
    auto capture=root.getChildFile("User capture & unicode "+String::charToString(0xe9)+".nam");capture.replaceWithText(payload().dump());const auto originalHash=SHA256(capture).toHexString();
    auto prepared=userpedals::prepare(capture,false);check(prepared.result.wasOk()&&prepared.model.dsp&&prepared.model.calibration.complete(),"prepare a calibrated personal capture without changing it");
    userpedals::Library library(root.getChildFile("User library"));
    check(library.store(3,prepared,"https://www.tone3000.com/tones/123").wasOk(),"store personal capture and provenance separately from factory Models");
    check(library.entries(3).size()==1&&library.entries(1).empty()&&SHA256(capture).toHexString()==originalHash,"per-pedal association and source bytes preserved");
    check(library.store(3,prepared).wasOk()&&library.entries(3).size()==1&&library.entry(3,originalHash).sourceUrl=="https://www.tone3000.com/tones/123","same capture is deduplicated by hash");
    auto partial=root.getChildFile("partial.nam");auto j=payload();j["metadata"].erase("output_level_dbu");partial.replaceWithText(j.dump());auto half=userpedals::prepare(partial,false);
    check(half.result.wasOk()&&!half.model.calibration.complete()&&half.model.calibration.input.has_value(),"partial calibration remains loadable for explicit warning approval");
    auto raw=root.getChildFile("uncalibrated.nam");raw.replaceWithText(payload(false).dump());auto uncalibrated=userpedals::prepare(raw,false);check(uncalibrated.result.wasOk()&&!uncalibrated.model.calibration.input&&!uncalibrated.model.calibration.output,"missing calibration is never fabricated");
    auto bad=root.getChildFile("invalid.nam");j=payload();j["metadata"]["input_level_dbu"]="invalid";bad.replaceWithText(j.dump());check(userpedals::prepare(bad,false).result.failed(),"malformed calibration rejected");
    j=payload();j["sample_rate"]=44100;bad.replaceWithText(j.dump());check(userpedals::prepare(bad,false).result.failed(),"incompatible training rate rejected before activation");
    const auto packFolder=root.getChildFile("Pack library");userpedals::Library packLibrary(packFolder);
    check(packLibrary.importPack(3,{{capture,"https://www.tone3000.com/tones/123"},{partial,{}}},false).wasOk()&&packLibrary.entries(3).size()==2,"complete pedal pack imported together without rejecting missing calibration");
    check(packLibrary.file(3,originalHash).getFileName()==capture.getFileName(),"personal files keep readable names and presets still resolve by content hash");
    const auto halfHash=SHA256(partial).toHexString();
    check(packLibrary.importPack(3,{{capture,{}},{partial,{}}},false).wasOk()&&packLibrary.entries(3).size()==2,"pack reimport deduplicates every existing capture");
    check(packLibrary.importPack(1,{{capture,{}},{bad,{}}},false).failed()&&packLibrary.entries(1).empty()&&packLibrary.folder(1).findChildFiles(File::findFiles,false,"*.nam").isEmpty(),"incompatible pack is rejected without a partial personal import");
    packLibrary.folder(1).createDirectory();const auto dropped=packLibrary.folder(1).getChildFile("Hand dropped capture.nam");capture.copyFileTo(dropped);
    check(packLibrary.refresh()&&packLibrary.entries(1).size()==1&&packLibrary.file(1,originalHash)==dropped,"folder additions automatically indexed with original filename and stable hash");
    check(!packLibrary.refresh(),"unchanged folder scan reuses validation without rebuilding DSPs");
    check(dropped.deleteFile()&&packLibrary.refresh()&&packLibrary.entries(1).empty(),"manual folder deletion removes the dropdown item");
    bad.copyFileTo(dropped);packLibrary.refresh();check(packLibrary.entries(1).empty()&&packLibrary.errors().isNotEmpty(),"incompatible manual NAM is excluded and diagnosed");dropped.deleteFile();packLibrary.refresh();
    check(packLibrary.remove(3,halfHash).wasOk()&&packLibrary.entries(3).size()==1&&partial.existsAsFile(),"delete removes only the selected personal capture and keeps original downloaded/source files");
    check(packLibrary.remove(0,originalHash).failed()&&packLibrary.entries(3).size()==1,"delete cannot target a factory or algorithmic pedal slot");
    check(userpedals::prepare(capture,false,String::repeatedString("a",64)).result.failed(),"changed saved capture cannot silently substitute a different model");
    const auto saved=library.file(3,originalHash);saved.replaceWithText("changed");check(userpedals::prepare(saved,false,originalHash).result.failed(),"corrupt installed personal capture detected");saved.replaceWithText(payload().dump());
    board::Processor processor;check(processor.load(models).wasOk(),"factory banks still load with original exact catalog checks");
    const auto factoryHash=processor.identity(3,0);processor.setCalibration({false,12.5f});processor.installCustom(3,std::move(prepared.model));
    for(int p=0;p<board::count;++p){auto setting=board::defaults(p,board::recommendedPreset(p));setting.enabled=false;processor.set(p,setting);}
    auto setting=board::defaults(3,board::presetCount(3));setting.enabled=true;setting.output=6;processor.set(3,setting);processor.front.set({0,-70,false});processor.prepareToPlay(48000,128);
    AudioBuffer<float> audio(2,128);MidiBuffer midi;for(int n=0;n<30;++n){for(int c=0;c<2;++c)for(int i=0;i<128;++i)audio.setSample(c,i,.02f);processor.processBlock(audio,midi);}
    check(std::abs(audio.getSample(0,127)-.02f*Decibels::decibelsToGain(6.f))<1.e-5f,"personal pedal retains intentional output boost without normalization");
    check(processor.identity(3,0)==factoryHash&&processor.identity(3,board::presetCount(3))==originalHash,"factory and personal identities remain independent");
    board::State state;state.name="Saved & shared "+String::charToString(0xe9);state.sourceType=amp::SourceType::Nam;state.namAmp.model={"Factory/Amp.nam",String::repeatedString("a",64),128};state.outputDsp=output::Settings{};
    for(int p=0;p<board::count;++p)state.pedals[size_t(p)]=board::defaults(p,board::recommendedPreset(p));state.pedals[3]=setting;
    board::RigIdentityLookup identities=[&](int p,int n){return processor.identity(p,n);};auto rig=board::encode(state,{},identities);board::State decoded;
    check(rig.getIntAttribute("version")==15&&board::decode(rig,{},identities,decoded).wasOk()&&decoded.customPedalHashes[3]==originalHash,"personal preset v15 persists hash, not menu position");
    auto oldBlues=rig;auto* blues=oldBlues.getChildByAttribute("id","10");blues->setAttribute("preset",15);blues->setAttribute("model",originalHash);blues->setAttribute("enabled",true);
    check(board::decode(oldBlues,{},identities,decoded).wasOk()&&decoded.pedals[10].preset==board::presetCount(10)&&decoded.customPedalHashes[10]==originalHash&&decoded.pedals[10].enabled,"previous Blues personal slot 15 migrates to the current personal slot without changing capture identity");
    blues->setAttribute("preset",14);blues->setAttribute("model","c4fb6b032d6149e16e555860cdd715651fefec59f917519d5fe6a2bd1a680836");
    check(board::decode(oldBlues,{},identities,decoded).wasOk()&&!decoded.pedals[10].enabled&&decoded.pedals[10].preset==board::recommendedPreset(10)&&decoded.customPedalHashes[10].isEmpty()&&!decoded.missingCaptureDiagnostics.isEmpty(),"withdrawn Waza restores the remaining rig with Blues bypassed and an explicit diagnostic");
    blues->setAttribute("personalCapture",true);
    check(board::decode(oldBlues,{},identities,decoded).wasOk()&&decoded.pedals[10].enabled&&decoded.customPedalHashes[10]==blues->getStringAttribute("model"),"explicit personal capture keeps its identity even if its bytes match the withdrawn factory Waza");
    const auto explicitPersonal=board::encode(decoded,{},identities);
    check(explicitPersonal.getChildByAttribute("id","10")->getBoolAttribute("personalCapture"),"new personal rig carries an unambiguous capture-kind marker");
    const board::RigIdentityLookup empty=[&](int p,int n){return board::isNam(p)&&n==board::presetCount(p)?String{}:processor.identity(p,n);};check(board::decode(rig,{},empty,decoded).wasOk()&&decoded.customPedalHashes[3]==originalHash,"personal capture identity can resolve after restart before a DSP is selected");
    auto broken=rig;broken.getChildByAttribute("id","3")->setAttribute("model","../../bad");check(board::decode(broken,{},identities,decoded).failed(),"untrusted personal identity rejected transactionally");
    state.customPedalHashes[3]=originalHash;const auto text=sharing::body(state,identities,"Capture: user pedal / source URL / hash "+originalHash);const URL email(sharing::mailto(text));
    check(text.contains(state.name)&&text.contains("<PedalRig")&&text.contains(originalHash)&&!text.contains("LevelCalibration")&&!text.contains(root.getFullPathName()),"sharing uses saved musical settings without device calibration or local absolute paths");
    check(email.getParameterValues()[email.getParameterNames().indexOf("body")]==text&&email.getParameterValues()[email.getParameterNames().indexOf("subject")]=="New preset suggestion","mailto preserves Unicode, XML, ampersands and newlines");
    t3k::Tone3000OAuthSession pedalSession("pedal");const URL auth(pedalSession.authorizationUrl());check(auth.getParameterValues()[auth.getParameterNames().indexOf("gears")]=="pedal","Tone3000 pedal selection is distinct from the existing amplifier flow");
    for(int v=0;v<board::count;++v)if(PedalCard::engineFor(v)==3)
    {
        PedalCard userCard(v,processor);userCard.setPersonalCaptures({{originalHash,"User capture"}});
        auto* combo=dynamic_cast<ComboBox*>(userCard.findChildWithID("pedal-preset"));check(combo&&combo->indexOfItemId(900001)>=0&&combo->indexOfItemId(900002)>=0&&combo->indexOfItemId(10000)>=0,"personal capture menu has factory items, personal items and both import actions");
        bool personalTrash=false,factoryProtected=true;
        for(PopupMenu::MenuItemIterator it(*combo->getRootMenu());it.next();){const auto& item=it.getItem();if(item.itemID==10000)personalTrash=dynamic_cast<PersonalCaptureRow*>(item.customComponent.get())!=nullptr;if(item.itemID>0&&item.itemID<10000&&item.customComponent)factoryProtected=false;}
        check(personalTrash&&factoryProtected&&combo->getItemText(combo->indexOfItemId(900002))=="Open pedal folder...","trash belongs only to personal rows and folder action replaces one-file import");
    }
    ampEq::Parameters eq;eq.lowCut=true;eq.highCut=true;eq.lowMidHz=350;eq.lowMidDb=4;eq.lowMidQ=.7f;eq.presenceHz=2600;eq.presenceDb=-3;eq.presenceQ=2;
    output::Settings fx;fx.ampEq=eq;auto encoded=output::encode(fx);output::Settings restored;
    check(encoded.getIntAttribute("version")==6&&output::decode(encoded,restored)&&restored.ampEq.lowCut&&restored.ampEq.presenceQ==2,"advanced EQ output state v6 round trip");
    state.pedals[3]=board::defaults(3,0);state.customPedalHashes[3].clear();state.outputDsp=fx;rig=board::encode(state,{},identities);
    check(rig.getIntAttribute("version")==14&&board::decode(rig,{},identities,decoded).wasOk(),"advanced EQ rig v14 round trip without personal captures");
    for(const auto badValue:{"nan","inf","-1","20001","3oops"}){auto invalid=encoded;invalid.getChildByName("AmpEq")->setAttribute("highCutHz",badValue);check(!output::decode(invalid,restored),"invalid advanced EQ value rejected: "+String(badValue));}
    const auto response=[](ampEq::Parameters p,double hz)
    {ampEq::Processor dsp;dsp.prepare(48000);dsp.setTarget(p,true);AudioBuffer<float> b(2,128);double in=0,out=0;for(int n=0;n<300;++n){for(int i=0;i<128;++i){const float x=float(.01*std::sin(2*MathConstants<double>::pi*hz*(n*128+i)/48000));b.setSample(0,i,x);b.setSample(1,i,-x);if(n>150)in+=double(x)*x;}dsp.process(b);if(n>150)for(int i=0;i<128;++i)out+=double(b.getSample(0,i))*b.getSample(0,i);}return 10*std::log10(out/in);};
    eq={};eq.lowCut=true;check(std::abs(response(eq,80)+3.0103)<.02,"native JUCE high-pass is -3 dB at cutoff");eq={};eq.highCut=true;check(std::abs(response(eq,8000)+3.0103)<.02,"native JUCE low-pass is -3 dB at cutoff");
    eq={};eq.lowMidDb=6;check(std::abs(response(eq,250)-6)<.02,"native JUCE low-mid peak gain at centre");eq={};eq.presenceDb=-6;check(std::abs(response(eq,3000)+6)<.02,"native JUCE presence cut at centre");
    ampEq::Processor dsp;dsp.prepare(48000);eq.lowCut=true;eq.highCut=true;dsp.setTarget(eq,true);realtimeProbe::begin();for(int n=0;n<100;++n){audio.clear();dsp.process(audio);}const auto eqAllocations=realtimeProbe::end();check(eqAllocations==0,"advanced stereo EQ performs zero allocations in process");
    dsp.setTarget(eq,false);for(int c=0;c<2;++c)for(int i=0;i<128;++i)audio.setSample(c,i,.02f);dsp.process(audio);check(audio.getSample(0,100)==.02f,"VST3 bypass remains exact with every advanced filter enabled");
    looping::Engine loop;loop.prepare(8000);AudioBuffer<float> b(2,257);auto pointers=b.getArrayOfWritePointers();
    const auto fill=[&](float left,float right){for(int i=0;i<257;++i){b.setSample(0,i,left);b.setSample(1,i,right);}};
    check(loop.advance()&&!loop.advance(),"looper commands are bounded and cannot overwrite an unconsumed command");
    realtimeProbe::begin();for(int n=0;n<16;++n){fill(.1f,.2f);loop.process(pointers,2,257);}const auto loopAllocations=realtimeProbe::end();
    check(loopAllocations==0&&loop.state()==looping::Mode::recording&&std::abs(loop.seconds()-4112./8000)<1.e-7,"looper records exact sample count without allocations");
    loop.advance();fill(.03f,.04f);loop.process(pointers,2,257);fill(.03f,.04f);loop.process(pointers,2,257);
    check(loop.state()==looping::Mode::playing&&std::abs(b.getSample(0,128)-.13f)<1.e-6f&&std::abs(b.getSample(1,128)-.24f)<1.e-6f,"stereo loop mixes with live processed sound without re-recording itself");
    for(int n=0;n<40;++n){fill(.05f,.06f);loop.process(pointers,2,257);}check(std::abs(b.getSample(0,128)-.15f)<1.e-6f,"changing live tone leaves recorded rhythm unchanged across wraps");
    loop.advance();fill(.03f,.04f);loop.process(pointers,2,257);check(loop.state()==looping::Mode::stopped&&b.getSample(0,128)==.03f&&loop.seconds()>0,"stop fades out and retains the loop");
    loop.advance();fill(0,0);loop.process(pointers,2,257);check(loop.state()==looping::Mode::playing&&b.getSample(0,128)>.09f,"resume keeps the same loop");loop.clear();fill(0,0);loop.process(pointers,2,257);check(loop.state()==looping::Mode::empty&&loop.seconds()==0,"clear releases the take without touching REC");
    loop.advance();for(int n=0;n<1900;++n){fill(.1f,.2f);loop.process(pointers,2,257);}check(loop.state()==looping::Mode::playing&&loop.seconds()==60,"60-second maximum automatically closes and plays the loop");loop.prepare(48000);check(loop.state()==looping::Mode::empty,"device/sample-rate restart clears incompatible loop memory");
    const paths::Layout linuxLayout(root.getChildFile("install/ToneGrid"),root.getChildFile("config"),paths::Platform::linuxOS);check(linuxLayout.encoder()==root.getChildFile("install/Tools/lame")&&linuxLayout.resources()==root.getChildFile("install"),"Linux uses native executable paths and portable resources");
    ampui::AdvancedEqControls controls;controls.sync(eq);check(controls.state().presenceDb==-6&&controls.state().lowCut,"EQ popup owns the same settings as the audio engine");
    report+=String(checks)+" Release122 checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
