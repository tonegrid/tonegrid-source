#pragma once
#include "TonexAmpSource.h"
#include "Vst3Identity.h"

namespace vst3
{
// Re-discover only the selected file on the message thread. Cache is a hint:
// cached name/version/bus metadata is never trusted to instantiate another class.
inline amp::LoadResult load(const juce::PluginDescription& requested,const juce::MemoryBlock* state=nullptr)
{
    using namespace juce;jassert(MessageManager::existsAndIsCurrentThread());
    try
    {
        if(!Identity::of(requested).valid())return {nullptr,Result::fail("Invalid VST3 identity."),{}};
        if(!locationExists(requested.fileOrIdentifier))return {nullptr,Result::fail("VST3 plugin missing: "+label(requested)),{}};
        auto formats=std::make_unique<AudioPluginFormatManager>();formats->addFormat(new VST3PluginFormat());
        OwnedArray<PluginDescription> types;formats->getFormat(0)->findAllTypesForFile(types,requested.fileOrIdentifier);
        const PluginDescription* found=nullptr;
        for(auto* type:types)if(Identity::of(*type)==Identity::of(requested))
        {if(found)return {nullptr,Result::fail("Ambiguous VST3 class identity in selected file."),{}};found=type;}
        if(!found)return {nullptr,Result::fail("Selected file no longer contains VST3: "+label(requested)),{}};
        const auto actual=*found;
        // Stable class identity, not the release string, controls compatibility.
        // A new candidate restores and prepares before replacing the current amp.
        String error;auto instance=formats->createPluginInstance(actual,48000,256,error);
        if(!instance)return {nullptr,Result::fail("VST3 load failed: "+error),{}};
        if(state)
        {
            if(state->isEmpty()||state->getSize()>45*1024*1024)return {nullptr,Result::fail("Invalid VST3 state."),{}};
            // JUCE provides no restore-status result. Exceptions are recoverable;
            // silent partial rejection by an opaque third-party plugin is not.
            restoreState(*instance,*state);
        }
        const auto stereo=amp::Vst3AmpHost::configureStereo(*instance,actual.name);
        if(stereo.failed())return {nullptr,stereo,{}};
        // Probe preparation of a NEW candidate only, outside the callback and
        // before replacing the live amp. The graph still prepares it normally.
        instance->setRateAndBufferSizeDetails(48000,256);instance->prepareToPlay(48000,256);instance->releaseResources();
        return {std::make_unique<amp::Vst3AmpHost>(std::move(formats),actual,std::move(instance),actual.name,true),Result::ok(),{}};
    }
    catch(const std::exception& e){return {nullptr,Result::fail("VST3 preparation/state failed: "+String(e.what())),{}};}
    catch(...){return {nullptr,Result::fail("VST3 preparation/state failed."),{}};}
}
}
