#pragma once
#include "Modules/Play/UI/RecordingControls.h"
#include "RealtimeAllocationProbe.h"

namespace recording114Tests
{
inline bool waitFor(const std::function<bool()>& test,int timeout=4000)
{const auto end=juce::Time::getMillisecondCounter()+timeout;while(!test()&&juce::Time::getMillisecondCounter()<end)juce::Thread::sleep(2);return test();}
inline bool run(const juce::File& lame,juce::String& report)
{
    using namespace juce;using namespace tonegrid::recording;
    bool ok=true;int count=0;auto check=[&](bool value,const String& label){++count;ok&=value;report+=String(value?"PASS ":"FAIL ")+"1.1.14 "+label+"\n";};
    const auto root=File::getSpecialLocation(File::tempDirectory).getChildFile("ToneGrid-114-test-"+Uuid().toString());root.createDirectory();
    SessionRecorder recorder(lame);AudioBuffer<float> input(2,128);int phase=0;
    auto feed=[&](int blocks,int delay=1)
    {
        for(int block=0;block<blocks;++block)
        {
            for(int i=0;i<128;++i){input.setSample(0,i,.18f*float(std::sin((phase+i)*.07)));input.setSample(1,i,.09f*float(std::cos((phase+i)*.11)));}
            phase+=128;recorder.process(input.getArrayOfWritePointers(),2,128);if(delay)Thread::sleep(delay);
        }
    };
    check(recorder.state()==Mode::empty&&!recorder.available()&&!recorder.play(48000)&&!recorder.exportMp3(root.getChildFile("empty.mp3")),"empty session disables play and export");
    input.clear();input.setSample(0,0,.375f);realtimeProbe::begin();recorder.process(input.getArrayOfWritePointers(),2,128);const auto idleAllocations=realtimeProbe::end();
    check(idleAllocations==0&&input.getSample(0,0)==.375f,"idle recorder leaves live audio unchanged without allocation");
    check(!recorder.record(0)&&!recorder.record(std::numeric_limits<double>::quiet_NaN()),"invalid sample rates rejected");
    check(recorder.record(48000)&&waitFor([&]{return recorder.state()==Mode::recording&&!recorder.requestPending();}),"starts stereo recording on worker");
    input.clear();input.setSample(0,0,.25f);input.setSample(1,0,-.125f);
    realtimeProbe::begin();recorder.process(input.getArrayOfWritePointers(),2,128);auto allocations=realtimeProbe::end();
    check(allocations==0&&input.getSample(0,0)==.25f&&input.getSample(1,0)==-.125f,"capture callback allocates nothing and preserves live output");
    feed(100);check(recorder.stop()&&waitFor([&]{return recorder.state()==Mode::ready&&!recorder.requestPending();}),"stop finalizes take before making it available");
    WavAudioFormat wav;std::unique_ptr<AudioFormatReader> reader(wav.createReaderFor(recorder.completedTake().createInputStream().release(),true));
    check(reader&&reader->numChannels==2&&reader->sampleRate==48000&&reader->bitsPerSample==24&&reader->lengthInSamples==12928,"stereo WAV duration, rate and 24-bit depth correct");
    AudioBuffer<float> readback(2,128);if(reader)reader->read(&readback,0,128,0,true,true);
    check(std::abs(readback.getSample(0,0)-.25f)<.000001f&&std::abs(readback.getSample(1,0)+.125f)<.000001f,"captured channels retain their distinct samples");reader.reset();
    check(recorder.play(48000)&&waitFor([&]{return recorder.state()==Mode::playing&&!recorder.requestPending();}),"play prepares the recording");
    input.clear();input.setSample(0,0,.9f);input.setSample(1,0,.9f);
    realtimeProbe::begin();recorder.process(input.getArrayOfWritePointers(),2,128);allocations=realtimeProbe::end();
    check(allocations==0&&std::abs(input.getSample(0,0)-.25f)<.00001f&&std::abs(input.getSample(1,0)+.125f)<.00001f,"playback bypasses live sound and allocates nothing");
    feed(110,2);check(waitFor([&]{return recorder.state()==Mode::ready;}),"play finishes once without looping");
    check(recorder.play(44100)&&waitFor([&]{return recorder.state()==Mode::playing&&!recorder.requestPending();}),"playback supports a changed device sample rate");
    feed(3);check(recorder.stop()&&waitFor([&]{return recorder.state()==Mode::ready&&!recorder.requestPending();}),"second play click stops and retains the take");
    recorder.play(48000);waitFor([&]{return recorder.state()==Mode::playing&&!recorder.requestPending();});input.clear();input.setSample(0,0,.9f);
    recorder.process(input.getArrayOfWritePointers(),2,128,true);check(input.getMagnitude(0,128)==0,"tuner mute also silences playback");recorder.stop();waitFor([&]{return recorder.state()==Mode::ready&&!recorder.requestPending();});
    const auto exported=root.getChildFile(String::fromUTF8("prise été & guitare.mp3"));
    check(recorder.exportMp3(exported)&&waitFor([&]{return recorder.state()==Mode::ready&&!recorder.requestPending();},20000),"MP3 export completes on worker");
    AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<AudioFormatReader> mp3reader(exported.existsAsFile()?formats.createReaderFor(exported):nullptr);
    check(mp3reader&&mp3reader->numChannels==2&&mp3reader->sampleRate==48000&&mp3reader->lengthInSamples>=12928,"exported MP3 decodes with stereo audio and the correct rate");
    AudioBuffer<float> decoded(2,4096);decoded.clear();if(mp3reader)mp3reader->read(&decoded,0,4096,0,true,true);
    check(decoded.getMagnitude(0,4096)>.01f&&decoded.getMagnitude(1,4096)>.01f,"decoded MP3 contains audible audio on both channels");mp3reader.reset();
    check(recorder.available()&&recorder.seconds()>0,"export retains the take for later play or export");
    exported.replaceWithText("replace this file");recorder.exportMp3(exported);
    check(waitFor([&]{return recorder.state()==Mode::ready&&!recorder.requestPending();},20000)&&exported.getSize()>1000&&exported.loadFileAsString()!="replace this file","approved overwrite replaces the destination with a completed MP3");
    check(!recorder.exportMp3(root.getChildFile("bad.wav")),"export requires the MP3 extension");
    const auto inaccessible=root.getChildFile("missing-directory/take.mp3");
    check(recorder.exportMp3(inaccessible)&&waitFor([&]{return recorder.state()==Mode::ready&&!recorder.requestPending();},20000)&&!inaccessible.existsAsFile()&&recorder.available(),"failed export retains recording and reports failure");
    check(recorder.takeMessage().contains("failed"),"export failure is visible to the user");
    check(recorder.play(48000)&&waitFor([&]{return recorder.state()==Mode::playing&&!recorder.requestPending();})&&recorder.record(48000)&&waitFor([&]{return recorder.state()==Mode::recording&&!recorder.requestPending();}),"REC during PLAY stops playback and starts a replacement");
    feed(5);recorder.stop();check(waitFor([&]{return recorder.state()==Mode::ready&&!recorder.requestPending();})&&std::abs(recorder.seconds()-640.0/48000)<.000001,"new REC replaces the duration and recording");
    recorder.record(48000);waitFor([&]{return recorder.state()==Mode::recording&&!recorder.requestPending();});feed(3);recorder.deviceStopped();
    check(waitFor([&]{return recorder.state()==Mode::ready;})&&recorder.available(),"device stop safely finalizes the recorded portion");
    File deletedTake;
    {
        SessionRecorder shortTake(lame,.025);shortTake.record(48000);waitFor([&]{return shortTake.state()==Mode::recording&&!shortTake.requestPending();});
        for(int i=0;i<12;++i){input.clear();shortTake.process(input.getArrayOfWritePointers(),2,128);Thread::sleep(1);}
        check(waitFor([&]{return shortTake.state()==Mode::ready;})&&shortTake.seconds()==.025,"automatic stop trims the final block exactly to the limit");deletedTake=shortTake.completedTake();
    }
    check(!deletedTake.existsAsFile(),"shutdown deletes the temporary recording");
    {
        SessionRecorder fresh(lame);check(!fresh.available()&&fresh.state()==Mode::empty,"next session starts without recording memory");
    }
    {
        SessionRecorder full(lame);full.record(8000);waitFor([&]{return full.state()==Mode::recording&&!full.requestPending();});
        AudioBuffer<float> large(2,4096);large.clear();
        // Still more than 50x faster than real 8 kHz capture, without turning
        // the duration test into a disk/FIFO-overflow stress test on slower PCs.
        for(int i=0;i<1172&&full.state()==Mode::recording;++i){full.process(large.getArrayOfWritePointers(),2,4096);Thread::sleep(10);}
        check(waitFor([&]{return full.state()==Mode::ready;})&&full.seconds()==600,"real 600-second limit tested with accelerated audio blocks");
        auto file=full.completedTake();std::unique_ptr<AudioFormatReader> r(file.existsAsFile()?wav.createReaderFor(file.createInputStream().release(),true):nullptr);
        check(r&&r->lengthInSamples==4800000,"ten-minute file has exactly the permitted number of frames");
    }
    root.deleteRecursively();report+=String(count)+" recording 1.1.14 checks\n";return ok;
}
}
