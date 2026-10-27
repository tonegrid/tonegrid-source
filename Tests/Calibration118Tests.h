#pragma once
#include "Effects/Pedalboard/PedalBoard.h"
#include "NamAmp.h"
#include "GuitarCalibrationWizard.h"
#include "CalibrationWarnings.h"
#include "Shared/UI/PopupWindow.h"
#include "AudioSettingsPanel.h"
#include "EffectsNodes.h"
#include "RealtimeAllocationProbe.h"
#include "TunerEngine.h"
#include "AmpSourceTests.h"

namespace calibration118Tests
{
inline nlohmann::json linear(std::optional<double> input={},std::optional<double> output={},float gain=1.f)
{
    nlohmann::json json={{"version","0.7.0"},{"architecture","Linear"},{"sample_rate",48000},{"config",{{"receptive_field",1},{"bias",false}}},{"weights",{gain}}};
    if(input)json["metadata"]["input_level_dbu"]=*input;if(output)json["metadata"]["output_level_dbu"]=*output;
    return json;
}
class Cubic final:public nam::DSP
{
public:
    Cubic():DSP(1,1,48000){}
    void Reset(double,int) override{}void prewarm() override{}
    void process(float** input,float** output,int count) override
    {for(int i=0;i<count;++i)output[0][i]=input[0][i]*input[0][i]*input[0][i];}
};
inline bool run(juce::String& report)
{
    using namespace juce;using namespace tonegrid::calibration;
    bool ok=true;int checks=0;
    auto check=[&](bool pass,const String& name){++checks;ok=ok&&pass;report+=(pass?"PASS ":"FAIL ")+name+"\n";};
    const auto closeEnough=[](float a,float b){return std::abs(a-b)<1.e-7f+1.e-4f*std::max(std::abs(a),std::abs(b));};
    const auto root=File::getSpecialLocation(File::tempDirectory).getChildFile("ToneGrid-calibration118-"+Uuid().toString());root.createDirectory();
    struct Cleanup{File file;~Cleanup(){if(file.isAChildOf(File::getSpecialLocation(File::tempDirectory)))file.deleteRecursively();}} cleanup{root};
    check(Settings{}.enabled&&!Settings{}.completed&&!valid(Settings{}),"fresh configuration defaults ON without inventing an interface reference");
    check(needsOnboarding(Settings{})&&!needsOnboarding(Settings{false,12.5f}),"first launch offers setup; confirmed OFF preference does not repeatedly trigger onboarding");
    check(metadata(nlohmann::json::array()).invalid,"non-object capture structure cannot be classified as calibrated");
    check(!parseReference("")&&!parseReference("NaN")&&!parseReference("12dBu")&&!parseReference("1e999")&&parseReference(" 12.5 ")==12.5f,"manual reference is strict and a blank never becomes zero");
    check(pedalGains({false,12},18,6).input==1&&pedalGains({false,12},18,6).output==1,"OFF is unity with complete metadata, without legacy 11.5 compensation");
    check(pedalGains({true,11.5},11.5,11.5).input==1,"legitimate 11.5 dBu capture/hardware values remain valid");
    for(const auto& item:std::initializer_list<std::pair<nlohmann::json,CaptureStatus>>{{linear(18,6),CaptureStatus::calibrated},{linear(18),CaptureStatus::partial},{linear({},6),CaptureStatus::partial},{linear(),CaptureStatus::uncalibrated}})
        check(metadata(item.first).status()==item.second,"metadata classification: "+metadata(item.first).description().upToFirstOccurrenceOf("\n",false,false));
    for(const auto& value:std::initializer_list<nlohmann::json>{"unknown",nullptr,true,1000,-1000,std::numeric_limits<double>::infinity()})
    {
        auto payload=linear(18,6);payload["metadata"]["input_level_dbu"]=value;
        check(metadata(payload).status()==CaptureStatus::invalid,"invalid optional value is reported, not silently classified as calibrated");
        sanitiseMetadata(payload);nam::DspLoadOptions options;options.prewarm=false;
        check(nam::get_dsp(payload,options)!=nullptr,"invalid optional data cannot reject the usable network");
    }
    auto malformed=linear();malformed["metadata"]="bad";check(metadata(malformed).invalid,"non-object metadata is invalid");sanitiseMetadata(malformed);
    check(!malformed.contains("metadata"),"local malformed metadata is sanitised");
    auto settings=Settings{true,12.5f};settings.profileId="scarlett-2i2-3";settings.deviceType="ASIO";settings.deviceName="Synthetic Focusrite";settings.channel=1;
    const auto preference=root.getChildFile("Audio/calibration.xml");
    check(write(preference,settings),"device calibration preference saved atomically");const auto restored=read(preference);
    check(restored.completed&&restored.enabled&&restored.profileId==settings.profileId&&restored.referenceDbU==12.5f&&restored.channel==1,"profile, reference, switch, completion, device and channel persist");
    check(interfaceStatus(restored)==InterfaceStatus::active&&interfaceStatus(restored,false)==InterfaceStatus::verification&&!restored.signalVerified,"old confirmed settings remain active; actual mismatch requires verification; no fabricated signal verification");
    check(matches(restored,"ASIO","Synthetic Focusrite",1)&&!matches(restored,"ASIO","Other",1)&&!matches(restored,"ASIO","Synthetic Focusrite",0),"exact same-device reconnect matches; changed device/channel requires verification");
    check(!write(preference,{true,INFINITY})&&read(preference).referenceDbU==12.5f,"invalid publication preserves previous preference");
    preference.replaceWithText("<LevelCalibration version=\"1\" enabled=\"1\" referenceDbU=\"12.5\"/>");
    check(read(preference).enabled&&!read(preference).completed&&!active(read(preference)),"old preference retains value but requires physical setup confirmation");
    preference.replaceWithText("<LevelCalibration version=\"2\" enabled=\"1\" referenceDbU=\"\"/>");check(!valid(read(preference)),"corrupt empty reference cannot become a calibrated zero");

    // Real board loader + real embedded NAM Linear networks in every pedal bank.
    // Variations exercise partial/missing/invalid paths through the production DSP.
    XmlElement catalog("PedalCatalog");
    for(int p=0;p<board::count;++p)if(board::isNam(p))
    {
        auto* bank=catalog.createNewChildElement("Pedal");bank->setAttribute("id",p);bank->setAttribute("recommended",board::recommendedPreset(p));
        for(int n=0;n<board::presetCount(p);++n)
        {
            auto payload=linear(p==board::id(board::PedalId::TrebleBoost)?15:18,p==board::id(board::PedalId::ClassicFuzz)?6:12);
            if(n==1)payload["metadata"].erase("output_level_dbu");
            if(n==2)payload["metadata"].erase("input_level_dbu");
            if(n==3)payload.erase("metadata");
            if(n==4)payload["metadata"]["output_level_dbu"]="invalid";
            const auto name=String(p)+"-"+String(n)+".nam";const auto file=root.getChildFile(name);file.replaceWithText(String(payload.dump()));
            auto* item=bank->createNewChildElement("Model");item->setAttribute("label","Synthetic "+name);item->setAttribute("file",name);item->setAttribute("sha256",SHA256(file).toHexString());
        }
    }
    root.getChildFile("catalog.xml").replaceWithText(catalog.toString());
    board::Processor board;const auto loaded=board.load(root);check(loaded.wasOk(),"production pedal loader accepts synthetic banks: "+loaded.getErrorMessage());
    if(loaded.failed())return false;
    board.setCalibration({true,12});
    auto model=std::make_unique<Cubic>();model->SetInputLevel(18);amp::NamAmpProcessor amp(std::move(model),{});
    amp.setCalibration({true,12});amp.setUpstreamReference(&board.referenceRouting);
    const int first=board::id(board::PedalId::ClassicFuzz),second=board::id(board::PedalId::TrebleBoost),boost=board::id(board::PedalId::CleanBoost),eq=board::id(board::PedalId::GraphicEq);
    auto enable=[&](int pedal,bool on,int preset=0,float artistic=0.f){auto state=board::defaults(pedal,preset);state.enabled=on;state.input=artistic;board.set(pedal,state);};
    enable(first,true);enable(second,true);board.prepareToPlay(48000,128);amp.prepareToPlay(48000,128);
    AudioBuffer<float> audio(2,128);MidiBuffer midi;
    auto render=[&](bool throughAmp=true)
    {
        for(int b=0;b<80;++b){for(int c=0;c<2;++c)for(int i=0;i<128;++i)audio.setSample(c,i,.25f);board.processBlock(audio,midi);if(throughAmp)amp.processBlock(audio,midi);}
        return audio.getSample(0,127);
    };
    const float boardExpected=.25f*Decibels::decibelsToGain(-6.f-6.f-3.f);
    check(closeEnough(render(false),boardExpected),"real NAM -> NAM board: signs and inter-model reference cancellation are correct");
    const float ampExpected=std::pow(boardExpected*Decibels::decibelsToGain(-6.f),3.f);
    check(closeEnough(render(),ampExpected),"multiple real NAM pedals -> nonlinear NAM amp: input calibrated once, not twice");
    amp.controls(6,0);check(closeEnough(render(),ampExpected*std::pow(Decibels::decibelsToGain(6.f),3.f)),"AMP GAIN remains before nonlinear inference");
    amp.controls(0,6);check(closeEnough(render(),ampExpected*Decibels::decibelsToGain(6.f)),"AMP VOLUME remains exclusively after nonlinear inference");amp.controls(0,0);
    enable(boost,true,0,6);check(closeEnough(render(false),boardExpected*Decibels::decibelsToGain(6.f)),"algorithmic clean boost retains its intentional +6 dB");
    enable(boost,false);enable(eq,true);check(closeEnough(render(false),boardExpected),"algorithmic neutral EQ preserves the reference");enable(eq,false);
    const int third=board::id(board::PedalId::TransparentOd),compressor=board::id(board::PedalId::StudioComp);
    enable(second,false);enable(third,true);auto compressorState=board::defaults(compressor,0);compressorState.enabled=true;compressorState.input=0;compressorState.ratio=1;compressorState.output=6;board.set(compressor,compressorState);
    check(closeEnough(render(false),.25f*Decibels::decibelsToGain(-12.f)),"NAM -> algorithmic +6 dB compressor makeup -> NAM preserves intentional intermediate gain");
    enable(compressor,false);enable(third,false);enable(second,true);
    ampTests::Trace vstTrace;auto plugin=std::make_unique<ampTests::Plugin>(vstTrace);plugin->gain=2;
    const auto description=plugin->getPluginDescription();amp::Vst3AmpHost vst(nullptr,description,std::move(plugin),"Synthetic VST3");
    juce::AudioProcessorGraph vstGraph;auto vstNode=vst.addToGraph(vstGraph);vstNode->getProcessor()->prepareToPlay(48000,128);
    render(false);vstNode->getProcessor()->processBlock(audio,midi);
    check(closeEnough(audio.getSample(0,127),boardExpected*2),"NAM pedal chain -> hosted VST3 processor receives the bus without NAM amplifier compensation");
    enable(first,false);check(closeEnough(render(false),.25f*Decibels::decibelsToGain(-3.f)),"bypassed NAM does not contribute its input or output compensation");
    enable(first,true,1);const auto partial=render(false);
    check(!board.completeCalibrationChain()&&closeEnough(partial,.25f*Decibels::decibelsToGain(-6.f)),"input-only pedal: known input is used, next unknown drive is not guessed");
    enable(second,false);render(false);check(!board.outputReferenceKnown.load(),"missing pedal output breaks the final amp input reference");
    check(closeEnough(render(),std::pow(.25f*Decibels::decibelsToGain(-6.f),3.f)),"amp skips input compensation after an unknown pedal output");
    enable(first,false);AudioBuffer<float> transition(2,4096);
    for(int c=0;c<2;++c)for(int i=0;i<4096;++i)transition.setSample(c,i,.25f);
    board.processBlock(transition,midi);
    check(!board.referenceRouting.at(0)&&board.referenceRouting.at(2048),"bypass transition within a block retains distinct reference state for each audio chunk");
    amp.processBlock(transition,midi);float largestStep=0;bool transitionFinite=true;
    for(int i=1;i<4096;++i){transitionFinite=transitionFinite&&std::isfinite(transition.getSample(0,i));largestStep=std::max(largestStep,std::abs(transition.getSample(0,i)-transition.getSample(0,i-1)));}
    check(transitionFinite&&largestStep<.002f,"capture/bypass transition remains finite with bounded smooth gain changes");
    enable(first,true,2);check(closeEnough(render(false),.25f*Decibels::decibelsToGain(-6.f))&&board.outputReferenceKnown.load(),"output-only pedal applies known output units without inventing input calibration");
    enable(first,true,3);check(closeEnough(render(false),.25f)&&!board.outputReferenceKnown.load(),"no metadata keeps raw compatibility and unknown outgoing reference");
    enable(first,true,4);check(closeEnough(render(false),.25f)&&board.calibrationInfo(first,4).invalid,"invalid calibration keeps a neutral stage with explicit invalid status");
    enable(first,true);enable(second,true);board.setCalibration({false,12});amp.setCalibration({false,12});
    check(closeEnough(render(false),.25f)&&closeEnough(render(),.015625f),"OFF removes all automatic compensation throughout real NAM pedal and amp chain");
    board.setCalibration({true,12});amp.setCalibration({true,12});enable(first,true,0,6);
    check(closeEnough(render(false),boardExpected*Decibels::decibelsToGain(6.f)),"pedal artistic input gain is preserved relative to calibration");enable(first,true);
    board.setNamLite(true);check(closeEnough(render(),ampExpected),"Full/Lite request on non-slimmable captures keeps correct calibration");
    for(int i=0;i<128;++i)audio.setSample(0,i,.1f);
    realtimeProbe::begin();board.processBlock(audio,midi);amp.processBlock(audio,midi);tonegrid::OutputPeak peak;peak.push(audio.getArrayOfReadPointers(),2,128);peak.take();const auto allocations=realtimeProbe::end();
    check(allocations==0,"production calibrated multi-stage callback and meters perform zero C++ allocations");
    // Compare a real A2 capture against independent calls to embedded NAM Core,
    // including its trained head_scale. This is not a formula-only oracle.
    const auto factory=File(TONEGRID_BANK_FILE).getParentDirectory().getParentDirectory().getChildFile("FactoryAmps");
    bool comparedA2=false;
    for(const auto& file:factory.findChildFiles(File::findFiles,true,"*.nam"))
    {
        auto payload=nlohmann::json::parse(file.loadFileAsString().toStdString());const auto info=metadata(payload);if(!info.input||info.invalid)continue;
        sanitiseMetadata(payload);nam::DspLoadOptions options;options.prewarm=false;
        auto probe=nam::get_dsp(payload,options);if(dynamic_cast<nam::SlimmableModel*>(probe.get())==nullptr)continue;
        for(const bool lite:{false,true})
        {
            auto expected=nam::get_dsp(payload,options);expected->SetPrewarmOnReset(false);expected->Reset(48000,128);tonegrid::namquality::apply(*expected,lite);expected->prewarm();
            amp::NamAmpProcessor actual(nam::get_dsp(payload,options),{{},3,-2},lite);actual.setCalibration({true,12});actual.prepareToPlay(48000,128);
            std::array<float,128> in{},out{};bool same=true;float maximumError=0;int mismatches=0;
            for(int b=0;b<24;++b)
            {
                for(int i=0;i<128;++i){const auto sample=.02f*std::sin(float((b*128+i)*2.0*juce::MathConstants<double>::pi*110/48000));audio.setSample(0,i,sample);in[size_t(i)]=sample*Decibels::decibelsToGain(3.f)*inputGain(12,info.input);}
                float* x=in.data();float* y=out.data();expected->process(&x,&y,128);actual.processBlock(audio,midi);
                for(int i=0;i<128;++i){const float reference=out[size_t(i)]*Decibels::decibelsToGain(-2.f);const float actualSample=audio.getSample(0,i);const bool close=closeEnough(actualSample,reference);same=same&&close;mismatches+=!close;maximumError=std::max(maximumError,std::abs(actualSample-reference));}
            }
            check(same,String("real A2 ")+(lite?"Lite":"Full")+" matches independent embedded NAM Core with single input calibration and retained trained scaling");
            if(!same)report+="A2_DIAGNOSTIC file="+file.getFullPathName()+" lite="+String(int(lite))+" mismatches="+String(mismatches)+" max_abs_error="+String(maximumError,12)+" input_dbu="+String(*info.input,8)+"\n";
            realtimeProbe::begin();actual.processBlock(audio,midi);const auto a2Allocations=realtimeProbe::end();check(a2Allocations==0,"real A2 callback has no C++ allocations");
        }
        comparedA2=true;break;
    }
    check(comparedA2,"available calibrated factory A2 capture exercises both native Full and Lite");
    const auto lstmFile=File(TONEGRID_BANK_FILE).getParentDirectory().getParentDirectory().getParentDirectory().getChildFile("vendor/NeuralAmpModelerCore-main/example_models/lstm.nam");
    check(lstmFile.existsAsFile(),"original NAM Core LSTM reference fixture is available (not part of the factory library)");
    if(lstmFile.existsAsFile())for(const auto* version:{"0.5.0","0.6.0","0.7.0"})
    {
        auto payload=nlohmann::json::parse(lstmFile.loadFileAsString().toStdString());payload["version"]=version;payload["sample_rate"]=48000;
        payload["metadata"]={{"input_level_dbu",18},{"output_level_dbu",6}};nam::DspLoadOptions options;options.prewarm=false;
        auto calibrated=nam::get_dsp(payload,options);payload.erase("metadata");auto raw=nam::get_dsp(payload,options);
        calibrated->SetPrewarmOnReset(false);raw->SetPrewarmOnReset(false);calibrated->Reset(48000,128);raw->Reset(48000,128);calibrated->prewarm();raw->prewarm();
        std::array<float,128> input{},out{},rawOut{};input.fill(.1f);float* x=input.data();float* y=out.data();float* z=rawOut.data();calibrated->process(&x,&y,128);raw->process(&x,&z,128);
        check(metadata(*calibrated).complete()&&metadata(*calibrated).input==18&&metadata(*calibrated).output==6,String("LSTM ")+version+" retains both references via common NAM loader");
        check(out==rawOut,String("LSTM ")+version+" metadata is read but NAM Core does not automatically apply calibration gains");
    }
    // The raw tap consumes inputs, not the graph output, irrespective of trim.
    tonegrid::tuning::Engine tuner;AudioProcessorPlayer player;tonegrid::tuning::AudioTap tap(player,tuner);
    AudioBuffer<float> rawInput(1,128),rawOutput(2,128);rawInput.clear();rawInput.setSample(0,64,.75f);
    tap.audioDeviceIOCallbackWithContext(rawInput.getArrayOfReadPointers(),1,rawOutput.getArrayOfWritePointers(),2,128,{});
    check(tap.takeRawInputPeak()==.75f&&tap.takeRawInputPeak()==0,"raw ADC meter captures pre-graph input independently of muted/empty graph output");
    rawInput.setSample(0,64,1.1f);tap.audioDeviceIOCallbackWithContext(rawInput.getArrayOfReadPointers(),1,rawOutput.getArrayOfWritePointers(),2,128,{});
    check(tap.takeRawInputPeak()==1.1f,"raw meter retains clipping peaks without modifying ADC samples");tap.audioDeviceStopped();check(!tap.inputConnected()&&tap.takeRawInputPeak()==0,"disconnection clears raw input meter state");
    Verification verification;
    for(int i=0;i<20;++i)verification.update({.0001f,true},i*.05);
    check(verification.status(1)==InputStatus::waiting&&!verification.safe(true,1),"steady background noise does not verify activity");
    verification.update({.03f,true},1);verification.update({.0001f,true},1.05);
    check(!verification.safe(true,1.05),"isolated click does not verify activity");
    verification.update({.001f,true},1.1);verification.update({.001f,true},1.2);
    check(verification.safe(true,1.2),"quiet -60 dBFS activity verifies without a target loudness");
    verification.update({.95f,true},2);check(verification.status(3)==InputStatus::clipping&&!verification.safe(true,3),"near clipping blocks saving during warning hold");
    verification.update({.0001f,true},4.1);check(!verification.safe(true,4.1),"clipping cannot become verified merely by waiting");
    verification.update({.03f,true},4.2);verification.update({.03f,true},4.3);check(verification.safe(true,4.3),"fresh safe activity recovers after clipping");
    verification.update({0,false},5);check(verification.status(5)==InputStatus::unavailable&&!verification.manualAvailable(9),"unavailable input cannot auto or manually verify");
    Verification quiet;quiet.update({.0000001f,true},0);quiet.update({.0000001f,true},2);
    check(quiet.status(2)==InputStatus::waiting&&quiet.manualAvailable(2),"very quiet input offers manual continuation without clipping or false verification");
    Verification invalid;invalid.update({std::numeric_limits<float>::quiet_NaN(),true},0);
    check(invalid.status(0)==InputStatus::clipping,"nonfinite meter data cannot verify activity");

    const auto profiles=interfaceProfiles();check(profiles.size()==46,"46 sourced or explicitly estimated interface groups embedded");
    for(const auto& legacy:std::initializer_list<String>{"scarlett-solo-3","scarlett-4i4-3","scarlett-solo-4","audient-evo4","ua-volt1","motu-m4","presonus-quantum-es4","arturia-minifuse1","arturia-minifuse4"})check(std::count_if(profiles.begin(),profiles.end(),[&](const auto& p){return p.matchesId(legacy);})==1,"legacy interface profile maps to one group: "+legacy);
    std::set<String> ids;for(const auto& p:profiles){check(ids.insert(p.id).second&&valid({true,p.reference})&&p.source.startsWith("https://"),"verified profile data: "+p.name);}
    for(const auto& p:profiles)
    {
        const auto copy=configureCopy(&p);
        const bool minimum=p.gainInstructions.isEmpty()&&p.settings.containsIgnoreCase("gain at minimum");
        check(copy.details==p.settings&&copy.minimum==minimum&&(p.gainInstructions.isNotEmpty()?copy.gain==p.gainInstructions:copy.minimum?copy.gain.contains("all the way down"):copy.gain.contains("control app")),"configure UI follows actual gain controls and preserves required settings: "+p.id);
    }
    for(const auto& expected:std::initializer_list<std::pair<const char*,float>>{{"maudio-air",6.f},{"maudio-air8",6.8f},{"maudio-air14",16.f},{"maudio-mtrack-solo-duo",6.f},{"fender-link-off",2.2f},{"fender-link-on",11.2f},{"behringer-umc22-um2",2.f},{"ssl12",14.f},{"rme-ufx-iii",21.f},{"motu-ultralite5",18.f}})
    {const auto found=std::find_if(profiles.begin(),profiles.end(),[&](const auto& p){return p.id==expected.first;});check(found!=profiles.end()&&std::abs(found->reference-expected.second)<.0001f,"instrument reference and distinct PAD/model variant: "+String(expected.first));}
    auto nonMinimum=profiles.front();nonMinimum.settings="Instrument input, physical gain at 12 o'clock, PAD off.";
    const auto otherPosition=configureCopy(&nonMinimum);
    check(!otherPosition.minimum&&otherPosition.details==nonMinimum.settings&&!otherPosition.gain.contains("fully left")&&!otherPosition.gainConfirmation.contains("fully down"),"a future non-minimum reference is not illustrated or claimed as minimum");
    const auto unknownHardware=configureCopy(nullptr);
    check(unknownHardware.minimum&&unknownHardware.gain.contains("fully left")&&unknownHardware.gain.contains("exact dBu value"),"manual hardware suggests minimum gain and distinguishes an exact reference");
    const auto warningsFile=root.getChildFile("warnings.xml");WarningStore warnings(warningsFile);
    const auto a=SHA256("first",5).toHexString(),b=SHA256("second",6).toHexString();
    check(warnings.shouldWarn(metadata(linear()),a)&&!warnings.shouldWarn(metadata(linear(12,12)),a),"only incomplete or invalid unique captures trigger warnings");
    check(warnings.acknowledge(a)&&!WarningStore(warningsFile).shouldWarn(metadata(linear()),a)&&WarningStore(warningsFile).shouldWarn(metadata(linear()),b),"content hash acknowledgement persists and does not confuse same-name different captures");
    check(warnings.acknowledge(a,true)&&!WarningStore(warningsFile).shouldWarn(metadata(linear()),b),"global warning suppression persists");

    GuitarWizard wizard({});DeviceInput fake{"Test","Synthetic guitar interface",{"Instrument 1","Instrument 2"},1,true};float livePeak=.000001f;Settings saved;int rawReads=0;double uiTime=0;wizard.now=[&]{return uiTime;};
    wizard.currentDevice=[&]{return fake;};wizard.reading=[&]{++rawReads;return RawReading{livePeak,fake.connected};};wizard.onSave=[&](Settings s){saved=s;return write(preference,s);};wizard.refresh();
    check(rawReads>0,"wizard drains raw peak accumulator before VERIFY to avoid stale activity or clipping");
    auto* choices=dynamic_cast<ComboBox*>(wizard.findChildWithID("calibration-profile"));auto* entry=dynamic_cast<TextEditor*>(wizard.findChildWithID("calibration-manual-reference"));
    check(wizard.findChildWithID("calibration-confirm-hiz")==nullptr&&wizard.findChildWithID("calibration-confirm-gain")==nullptr,"hardware confirmation checkboxes removed");
    check(wizard.findChildWithID("calibration-confirm-signal")==nullptr,"manual signal confirmation checkbox removed");auto* next=dynamic_cast<TextButton*>(wizard.findChildWithID("calibration-next"));
    check(wizard.currentStep()==0&&choices->getItemText(0)=="OTHER INTERFACE / MANUAL"&&choices->getNumItems()==47,"wizard starts separately at UNDERSTAND; manual first and 46 grouped presets");
    check(next->getWantsKeyboardFocus()&&next->getExplicitFocusOrder()>0,"wizard buttons participate in explicit keyboard traversal");
    const auto previewFolder=File::getSpecialLocation(File::currentExecutableFile).getSiblingFile("Calibration118Tests");previewFolder.createDirectory();
    auto screenshot=[&](Component& body,const String& name,const String& title)
    {tonegrid::popup::Frame frame(body,false,title,0);MemoryOutputStream stream;PNGImageFormat().writeImageToStream(frame.createComponentSnapshot(frame.getLocalBounds()),stream);check(previewFolder.getChildFile(name+".png").replaceWithData(stream.getData(),stream.getDataSize()),"UI preview "+name);};
    screenshot(wizard,"wizard-understand","Guitar Calibration");wizard.advance();
    check(wizard.currentStep()==1&&parseReference(entry->getText())==12.f&&next->isEnabled(),"CONFIGURE offers 12 dBu estimate and continues without checkbox gates");
    check(wizard.findChildWithID("calibration-minimum-gain")->isVisible(),"manual profile illustrates the suggested minimum-gain starting point");
    screenshot(wizard,"wizard-configure-manual","Guitar Calibration");
    for(const auto& p:profiles)if(p.id=="fender-link-off"||p.id=="fender-link-on"||p.id=="rme-ufx-iii"||p.id=="yamaha-ag03mk2"||p.estimated)
    {
        choices->setSelectedId(int(&p-profiles.data())+2,sendNotificationSync);
        check(parseReference(entry->getText())==p.reference,"variant selection uses its own reference: "+p.id);
        if(p.gainInstructions.isNotEmpty())check(!wizard.findChildWithID("calibration-minimum-gain")->isVisible()&&dynamic_cast<Label*>(wizard.findChildWithID("calibration-gain-explanation"))->getText()==p.gainInstructions,"fixed/software/switch gain instructions replace physical-knob advice: "+p.id);
        if(p.estimated)check(dynamic_cast<Label*>(wizard.findChildWithID("calibration-value-explanation"))->getText().contains("Starting estimate"),"unverified M-Track reference is explicitly marked as an estimate");
        screenshot(wizard,"wizard-configure-"+p.id,"Guitar Calibration");
    }
    for(const auto& p:profiles)if(p.id=="audient-evo8"||p.id=="motu-m2"||p.id=="ssl2-original"||p.id=="scarlett-2i2-4")
    {
        const int index=int(&p-profiles.data());choices->setSelectedId(index+2,sendNotificationSync);
        const auto inputCopy=dynamic_cast<Label*>(wizard.findChildWithID("calibration-input-explanation"))->getText();
        check(parseReference(entry->getText())==p.reference&&wizard.findChildWithID("calibration-minimum-gain")->isVisible()&&next->isEnabled()==p.supports(fake.channel),"interface switch updates reference/illustration and validates the instrument channel: "+p.id);
        check(dynamic_cast<Label*>(wizard.findChildWithID("calibration-required-settings"))->getText()==p.settings&&!wizard.findChildWithID("calibration-required-settings")->isVisible(),"technical settings retained separately from beginner copy: "+p.id);
        if(p.id=="audient-evo8"||p.id=="motu-m2")check(inputCopy.containsIgnoreCase("no INST switch is needed"),"front instrument connection does not demand a nonexistent INST switch: "+p.id);
        screenshot(wizard,"wizard-configure-"+p.id,"Guitar Calibration");
    }
    const auto scarlett=std::find_if(profiles.begin(),profiles.end(),[](const auto& p){return p.id=="scarlett-2i2-3";});const int scarlettChoice=int(scarlett-profiles.begin())+2;choices->setSelectedId(scarlettChoice,sendNotificationSync);
    check(entry->isReadOnly()&&parseReference(entry->getText())==12.5f&&next->isEnabled(),"selected Scarlett 2i2 3rd Gen uses actual +12.5 dBu and matching channel");
    check(dynamic_cast<Label*>(wizard.findChildWithID("calibration-input-explanation"))->getText().contains("INST on")&&dynamic_cast<Label*>(wizard.findChildWithID("calibration-gain-explanation"))->getText().contains("AIR, PAD")&&dynamic_cast<Label*>(wizard.findChildWithID("calibration-gain-explanation"))->getText().contains("minimum"),"concise Scarlett instructions retain INST, AIR/PAD OFF and minimum gain");
    check(!wizard.findChildWithID("calibration-value-explanation")->isVisible(),"known interface avoids redundant reference explanation");
    auto* dbuHelp=dynamic_cast<Button*>(wizard.findChildWithID("calibration-dbu-help"));dbuHelp->onClick();
    check(dbuHelp->getWantsKeyboardFocus()&&dynamic_cast<Label*>(wizard.findChildWithID("calibration-dbu-explanation"))->getText().startsWith("dBu describes your interface's input sensitivity."),"dBu help opens in ToneGrid and supports keyboard focus");
    screenshot(wizard,"wizard-configure-dbu-help","Guitar Calibration");dbuHelp->onClick();
    {
        tonegrid::popup::Frame laptop(wizard,false,"Guitar Calibration",0);laptop.setSize(laptop.getWidth(),680);
        auto* viewport=wizard.findParentComponentOfClass<Viewport>();auto* footer=laptop.findChildWithID("popup-footer");
        for(const auto size:std::initializer_list<Point<int>>{{684,618},{600,420},{360,340}})
        {
            laptop.setSize(size.x,size.y);viewport->setViewPosition(0,0);const auto fixed=laptop.getLocalArea(next,next->getLocalBounds());
            check(footer&&next->getParentComponent()==footer&&laptop.getLocalBounds().contains(fixed)&&fixed.getY()>=viewport->getBottom()&&next->isEnabled(),"NEXT fixed and visible at "+String(size.x)+"x"+String(size.y));
            viewport->setViewPosition(0,10000);check(laptop.getLocalArea(next,next->getLocalBounds())==fixed,"scrolling never moves NEXT");
            int checkboxes=0;for(auto* child:wizard.getChildren())checkboxes+=dynamic_cast<ToggleButton*>(child)!=nullptr;check(checkboxes==0,"no checkbox in calibration wizard");
        }
        laptop.setSize(684,618);viewport->setViewPosition(0,0);
        auto traverser=laptop.createKeyboardFocusTraverser();const auto focusable=traverser->getAllComponents(&laptop);check(std::find(focusable.begin(),focusable.end(),next)!=focusable.end(),"fixed NEXT remains in keyboard traversal");
        MemoryOutputStream image;PNGImageFormat().writeImageToStream(laptop.createComponentSnapshot(laptop.getLocalBounds(),true,1.5f),image);
        check(previewFolder.getChildFile("wizard-configure-laptop-scale150.png").replaceWithData(image.getData(),image.getDataSize()),"laptop configure preview at 150 percent");
    }
    screenshot(wizard,"wizard-configure","Guitar Calibration");wizard.advance();wizard.refresh();
    uiTime=.3;wizard.refresh();livePeak=.001f;uiTime=.4;wizard.refresh();uiTime=.5;wizard.refresh();
    check(wizard.currentStep()==2&&next->isEnabled(),"VERIFY automatically accepts sustained quiet activity");
    screenshot(wizard,"wizard-verify","Guitar Calibration");wizard.advance();screenshot(wizard,"wizard-saved","Guitar Calibration");
    check(saved.signalVerified&&read(preference).signalVerified&&interfaceStatus(saved)==InterfaceStatus::active,"automatic signal verification persists separately from active interface state");
    check(saved.completed&&saved.channel==1&&saved.deviceName==fake.name&&saved.enabled&&read(preference).completed,"wizard saves confirmed global device profile and ON preference");
    GuitarWizard offWizard(saved);offWizard.now=[&]{return uiTime;};offWizard.currentDevice=[&]{return fake;};offWizard.reading=[&]{return RawReading{.01f,true};};offWizard.onSave=[&](Settings s){return write(preference,s);};offWizard.refresh();offWizard.advance();
    auto* offChoices=dynamic_cast<ComboBox*>(offWizard.findChildWithID("calibration-profile"));offChoices->setSelectedId(1,sendNotificationSync);
    auto* offEntry=dynamic_cast<TextEditor*>(offWizard.findChildWithID("calibration-manual-reference"));
    check(parseReference(offEntry->getText())==12.f,"returning to manual uses 12 dBu estimate instead of inheriting a manufacturer value");offEntry->setText("12.5");
    offChoices->setSelectedId(scarlettChoice,sendNotificationSync);offChoices->setSelectedId(1,sendNotificationSync);
    check(parseReference(offEntry->getText())==12.5f,"explicit manual entry is retained when browsing manufacturer profiles");
    offWizard.advance();offWizard.refresh();
    auto* offAutomatic=dynamic_cast<TextButton*>(offWizard.findChildWithID("calibration-automatic"));offAutomatic->setToggleState(false,dontSendNotification);offAutomatic->onClick();
    uiTime+=2;offWizard.refresh();auto* manualNext=dynamic_cast<TextButton*>(offWizard.findChildWithID("calibration-next"));
    check(manualNext->isEnabled()&&offWizard.findChildWithID("calibration-manual-continue")==nullptr,"steady input can save settings without another checkbox");
    screenshot(offWizard,"wizard-manual","Guitar Calibration");offWizard.advance();
    check(!read(preference).signalVerified&&dynamic_cast<Label*>(offWizard.findChildWithID("calibration-feedback"))->getText()=="Saved without a signal check.","unverified input remains honestly marked after saving");
    screenshot(offWizard,"wizard-saved-manual-off","Guitar Calibration");
    check(!read(preference).enabled&&read(preference).completed,"wizard OFF selection persists with the confirmed manual hardware reference");
    GuitarWizard changed(saved);changed.currentDevice=[&]{return fake;};changed.reading=[&]{return RawReading{.2f,true};};changed.refresh();changed.advance();
    choices=dynamic_cast<ComboBox*>(changed.findChildWithID("calibration-profile"));
    changed.advance();fake.name="Different device";changed.refresh();
    check(changed.currentStep()==1&&dynamic_cast<TextButton*>(changed.findChildWithID("calibration-next"))->isEnabled(),"device change during VERIFY returns to CONFIGURE for the new device");
    const String longFilename="Blackface Fender Super Reverb - A very long capture filename with original capitalization and additional information - Full Rig Amp and Cabinet by Creator.nam";
    CaptureWarning warning(metadata(linear(12)),longFilename);check(!warning.popupBranding(),"external warning has no ToneGrid logo");screenshot(warning,"capture-warning","Capture Calibration");
    check(dynamic_cast<Label*>(warning.findChildWithID("capture-calibration-filename"))->getText()==longFilename,"full actual filename including .nam is retained");
    check(!dynamic_cast<TextEditor*>(warning.findChildWithID("capture-calibration-message"))->getText().contains("dBu"),"raw metadata block removed from main message");
    auto* link=dynamic_cast<Button*>(warning.findChildWithID("capture-calibration-learn"));check(link&&link->getWantsKeyboardFocus()&&link->getExplicitFocusOrder()==1,"text link is keyboard accessible");link->onClick();
    check(dynamic_cast<TextEditor*>(warning.findChildWithID("capture-calibration-message"))->getText().contains("real guitar voltages"),"Learn More opens an actual in-app explanation");
    screenshot(warning,"capture-details","Capture Calibration");
    bool continued=false,suppressed=false;warning.onUse=[&](bool value){continued=true;suppressed=value;};
    auto* use=dynamic_cast<TextButton*>(warning.findChildWithID("capture-calibration-use"));auto* suppress=dynamic_cast<ToggleButton*>(warning.findChildWithID("capture-calibration-suppress"));
    check(use->getButtonText()=="OK, Continue"&&use->isRegisteredForShortcut(KeyPress(KeyPress::returnKey))&&use->getX()==8,"Continue is bottom-left and Return default");
    check(suppress->getButtonText()=="Don't show this again (all captures)"&&!suppress->getToggleState(),"global preference label and unchecked default unchanged");
    suppress->setToggleState(true,dontSendNotification);use->onClick();check(continued&&suppressed,"Continue forwards unchanged global preference behavior");
    for(int i=0;i<4;++i)
    {
        const auto info=i==0?metadata(linear()):i==1?metadata(linear(12)):i==2?metadata(linear({},6)):metadata(linear(200,6));
        CaptureWarning item(info,"Actual Capture "+String(i)+".nam");
        const auto label=dynamic_cast<Label*>(item.findChildWithID("capture-calibration-status"))->getText();
        check(label==(i==0?"Uncalibrated":i==3?"Invalid calibration information":"Partially calibrated"),"distinct capture status "+String(i));
        screenshot(item,"capture-status-"+String(i),"Capture Calibration");
    }
    report+=String(checks)+" calibration 1.1.18 checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
