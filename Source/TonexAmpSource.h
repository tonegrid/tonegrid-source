#pragma once
#include "Vst3AmpHost.h"
#include "PluginLocations.h"

namespace amp
{
// Current product policy only: known file, TONEX name filter, VST3, stereo.
// No directory scan, cache, generic selection or new source type is activated.
inline juce::String tonexDefaultPath() { return vst3::defaultTonexPath(); }
struct LoadResult
{
    std::unique_ptr<AmpSource> source;
    juce::Result result=juce::Result::ok();
    juce::String statusMessage;
};
inline LoadResult loadTonex(const juce::File& file,double sampleRate=48000,int blockSize=256)
{
    using namespace juce;
    jassert(MessageManager::existsAndIsCurrentThread());
    if(!file.exists())return {nullptr,Result::fail("TONEX path does not exist. Use Open Amp to locate TONEX.vst3."),"TONEX not found - use Open Amp to locate it."};
    auto formats=std::make_unique<AudioPluginFormatManager>();formats->addFormat(new VST3PluginFormat());
    OwnedArray<PluginDescription> types;
    formats->getFormat(0)->findAllTypesForFile(types,file.getFullPathName());
    PluginDescription* found=nullptr;
    for(auto* candidate:types)if(candidate->name.containsIgnoreCase("TONEX")){found=candidate;break;}
    if(!found)return {nullptr,Result::fail("No TONEX VST3 found. Use Open Amp to locate it."),{}};
    const auto description=*found;
    String error;auto instance=formats->createPluginInstance(description,sampleRate,blockSize,error);
    if(!instance)return {nullptr,Result::fail("Load failed: "+error),{}};
    const auto stereo=Vst3AmpHost::configureStereo(*instance,"TONEX");
    if(stereo.failed())return {nullptr,stereo,{}};
    return {std::make_unique<Vst3AmpHost>(std::move(formats),description,std::move(instance),"TONEX"),Result::ok(),{}};
}
}
