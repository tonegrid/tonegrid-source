#pragma once
#include "Platform/PlatformFiles.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
namespace tonegrid::core {
struct IMp3Encoder {virtual juce::Result encode(const juce::File& wav,const juce::File& target,const std::atomic<bool>& cancelled)=0;virtual ~IMp3Encoder()=default;};
class LameMp3Encoder final:public IMp3Encoder {
public:explicit LameMp3Encoder(juce::File binary):executable(std::move(binary)){}
 juce::Result encode(const juce::File& wav,const juce::File& target,const std::atomic<bool>& cancelled)override {
  using namespace juce;if(!executable.existsAsFile())return Result::fail(platform::missingEncoderMessage());
  TemporaryFile staged(target);ChildProcess child;const StringArray args{executable.getFullPathName(),"--quiet","--cbr","-b","256",wav.getFullPathName(),staged.getFile().getFullPathName()};
  if(!child.start(args,0))return Result::fail("Could not start the MP3 encoder.");
  while(child.isRunning()){if(cancelled){child.kill();child.waitForProcessToFinish(2000);return Result::fail("Export cancelled. Your recordings are intact.");}child.waitForProcessToFinish(30);}
  if(child.getExitCode()!=0||staged.getFile().getSize()<100)return Result::fail("MP3 encoding failed. Your recordings are intact.");
  AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<AudioFormatReader> check(formats.createReaderFor(staged.getFile()));
  if(!check||check->lengthInSamples<=0)return Result::fail("The MP3 encoder returned an invalid file.");
  if(cancelled)return Result::fail("Export cancelled.");if(!platform::commitExport(staged.getFile(),target))return Result::fail("Could not publish the MP3. Existing destination was preserved.");return Result::ok();
 }
private:juce::File executable;
};
}
