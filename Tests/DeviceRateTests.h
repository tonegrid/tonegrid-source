#pragma once
#include "DeviceRateProcessor.h"
#include "NamAmpTests.h"
#include "RawInputTestDevice.h"
#include "TunerEngine.h"
#include "RealtimeAllocationProbe.h"
namespace deviceRateTests
{
inline bool run(juce::String& report)
{
    using namespace juce;using tonegrid::audio::DeviceRateProcessor;
    bool ok=true;int checks=0;
    auto check=[&](bool pass,const String& name){++checks;ok&=pass;const auto line=String(pass?"PASS ":"FAIL ")+"DeviceRate "+name+"\n";report+=line;std::cout<<line<<std::flush;};
    check(DeviceRateProcessor::supported(44100)&&DeviceRateProcessor::supported(192000)&&!DeviceRateProcessor::supported(NAN)&&!DeviceRateProcessor::supported(0),"valid device rates accepted, invalid rejected");
    ampTests::Trace trace;ampTests::Plugin vst(trace);vst.gain=.5f;
    DeviceRateProcessor adapter(vst);MidiBuffer midi;
    for(double rate:{44100.,48000.,88200.,96000.,192000.})
    {
        adapter.prepareToPlay(rate,96);
        check(vst.getSampleRate()==48000&&adapter.converting()==(rate!=48000)&&((rate==48000)==(adapter.conversionLatency()==0)),"VST-compatible engine rate / measured latency at "+String(rate));
        int position=0,crossings=0,count=0;float last=0,peak=0;bool finite=true,exact=true;double energy=0;
        const std::array<int,7> sizes{1,17,96,128,513,2049,7};
        for(int b=0;position<int(rate*.3);++b)
        {
            const int n=sizes[size_t(b)%sizes.size()];AudioBuffer<float> audio(2,n);
            for(int i=0;i<n;++i){const float x=float(.2*std::sin(2*MathConstants<double>::pi*1000*(position+i)/rate));audio.setSample(0,i,x);audio.setSample(1,i,-x);}
            adapter.processBlock(audio,midi);
            for(int i=0;i<n;++i)
            {
                const float x=float(.2*std::sin(2*MathConstants<double>::pi*1000*(position+i)/rate));
                const float y=audio.getSample(0,i);finite&=std::isfinite(y)&&std::abs(y)<.12f&&std::abs(y+audio.getSample(1,i))<1.e-6f;
                if(rate==48000)exact&=y==x*.5f;
                if(position+i>int(rate*.05)){if(last<=0&&y>0)++crossings;energy+=y*y;++count;peak=jmax(peak,std::abs(y));}last=y;
            }
            position+=n;
        }
        const double hz=crossings*rate/count,rms=std::sqrt(energy/count);
        report+="  measured Hz="+String(hz,2)+" RMS="+String(rms,5)+" latency="+String(adapter.conversionLatency())+" samples\n";
        check(finite&&std::abs(hz-1000)<8&&std::abs(rms-.0707107)<.001&&peak>.095f&&!adapter.fault.load(),"pitch, gain, stereo and irregular/oversized blocks at "+String(rate));
        check(exact,"48k bypass is sample-exact at "+String(rate));
        AudioBuffer<float> audio(2,128);audio.clear();adapter.processBlock(audio,midi);
        realtimeProbe::begin();for(int i=0;i<100;++i){audio.clear();adapter.processBlock(audio,midi);}const auto allocations=realtimeProbe::end();
        check(allocations==0,"no allocations in 100 prepared callbacks at "+String(rate));adapter.releaseResources();
    }
    // Real JUCE player, stereo graph, pedal processor and NAM amplifier.
    rig::StereoGraph graph;graph.setPlayConfigDetails(2,2,48000,256);using IO=AudioProcessorGraph::AudioGraphIOProcessor;
    auto in=graph.addNode(std::make_unique<IO>(IO::audioInputNode)),out=graph.addNode(std::make_unique<IO>(IO::audioOutputNode));
    auto pedals=std::make_unique<board::Processor>();
    // A synthetic validated pedal catalogue tests the real board engine without user assets.
    const auto temp=File::getSpecialLocation(File::tempDirectory).getChildFile("ToneGrid-rate-"+Uuid().toString());
    temp.createDirectory();ScopeGuard cleanup{[&]{temp.deleteRecursively();}};
    XmlElement catalog("PedalCatalog");
    for(int p=0;p<board::count;++p)if(board::isNam(p))
    {
        auto* bank=catalog.createNewChildElement("Pedal");bank->setAttribute("id",p);bank->setAttribute("recommended",board::recommendedPreset(p));
        for(int n=0;n<board::presetCount(p);++n)
        {
            const auto file=temp.getChildFile(String(p)+"-"+String(n)+".nam");file.replaceWithText(namTests::linear());
            auto* item=bank->createNewChildElement("Capture");item->setAttribute("file",file.getFileName());item->setAttribute("label","Fixture");item->setAttribute("sha256",SHA256(file).toHexString());
        }
    }
    catalog.writeTo(temp.getChildFile("catalog.xml"));check(pedals->load(temp).wasOk(),"real pedalboard loaded synthetic NAM banks");
    for(int p:{board::id(board::PedalId::HorseOd),board::id(board::PedalId::TubeScreamer)}){auto s=pedals->get(p);s.enabled=true;pedals->set(p,s);}
    auto setting=pedals->get(board::id(board::PedalId::CleanBoost));setting.enabled=true;setting.input=6;pedals->set(board::id(board::PedalId::CleanBoost),setting);
    auto pnode=graph.addNode(std::move(pedals));
    nam::DspLoadOptions options;options.prewarm=false;
    auto model=nam::get_dsp(nlohmann::json::parse(namTests::linear(.5f).toStdString()),options);
    auto amp=graph.addNode(std::make_unique<amp::NamAmpProcessor>(std::move(model),namamp::State{}));
    for(int c=0;c<2;++c){graph.addConnection({{in->nodeID,c},{pnode->nodeID,c}});graph.addConnection({{pnode->nodeID,c},{amp->nodeID,c}});graph.addConnection({{amp->nodeID,c},{out->nodeID,c}});}
    DeviceRateProcessor device(graph);AudioProcessorPlayer player;tonegrid::tuning::Engine tuner;tonegrid::tuning::AudioTap tap(player,tuner);
    tonegrid::looping::Engine loop;tap.setLooper(&loop);
    tonegrid::recording::SessionRecorder recorder({});tap.setRecorder(&recorder);
    RawInputTestDevice hardware(44100,96);tap.audioDeviceAboutToStart(&hardware);player.setProcessor(&device);
    check(graph.getSampleRate()==48000&&recorder.record(44100),"44.1k-only device drives fixed-rate graph and recording starts");
    const auto deadline=Time::getMillisecondCounter()+3000;while(recorder.state()!=tonegrid::recording::Mode::recording&&Time::getMillisecondCounter()<deadline)Thread::sleep(2);
    loop.advance();AudioBuffer<float> input(1,96),output(2,96);double power=0;bool finite=true;
    for(int b=0;b<300;++b)
    {
        for(int i=0;i<96;++i)input.setSample(0,i,float(.02*std::sin(2*MathConstants<double>::pi*220*(b*96+i)/44100)));
        tap.audioDeviceIOCallbackWithContext(input.getArrayOfReadPointers(),1,output.getArrayOfWritePointers(),2,96,{});
        if(b>40)for(int i=0;i<96;++i){const auto x=output.getSample(0,i);finite&=std::isfinite(x)&&x==output.getSample(1,i);power+=x*x;}
        if(b%10==0)Thread::sleep(1);
    }
    const double rms=std::sqrt(power/(259*96));
    check(finite&&std::abs(rms-(.02*.5*std::pow(10.,6./20)/std::sqrt(2.)))<.0005,"NAM pedals, intentional +6 dB algorithmic boost and NAM amp audible at 44.1k");
    check(std::abs(loop.seconds()-300.*96/44100)<1.e-8&&tap.takeRawInputPeak()>.019f&&tap.takeOutputPeak()>.019f,"looper duration and raw/final meters use device domain");
    loop.advance();input.clear();for(int b=0;b<30;++b)tap.audioDeviceIOCallbackWithContext(input.getArrayOfReadPointers(),1,output.getArrayOfWritePointers(),2,96,{});
    check(loop.state()==tonegrid::looping::Mode::playing&&output.getMagnitude(0,96)>.001f,"post-chain loop playback remains audible");
    recorder.stop();const auto stopDeadline=Time::getMillisecondCounter()+3000;while(recorder.state()!=tonegrid::recording::Mode::ready&&Time::getMillisecondCounter()<stopDeadline)Thread::sleep(2);
    AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<AudioFormatReader> reader(formats.createReaderFor(recorder.completedTake()));
    check(reader&&reader->sampleRate==44100&&reader->numChannels==2&&reader->lengthInSamples>=300*96,"recorded WAV retains actual device rate and length");
    tap.audioDeviceStopped();player.setProcessor(nullptr);
    // Repeated device-rate changes do not retime NAM models or retain stale resampler output.
    for(double rate:{96000.,44100.,48000.}){device.prepareToPlay(rate,128);AudioBuffer<float> silence(2,513);silence.clear();device.processBlock(silence,midi);check(graph.getSampleRate()==48000&&!device.fault.load()&&silence.getMagnitude(0,513)<1.e-8f,"restart at "+String(rate)+" clears conversion history");device.releaseResources();}
    report+=String(checks)+" DeviceRate checks; "+(ok?"ALL PASSED":"FAILURES")+"\n";return ok;
}
}
