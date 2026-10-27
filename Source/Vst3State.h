#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace vst3
{
// Generic hosted state = opaque JUCE blob plus writable automation values keyed
// by stable VST3 parameter ID. E.g. Genome's A1 macro is not restored by its blob
// alone on the same instance. Legacy TONEX capture remains byte-for-byte raw.
inline constexpr char stateMagic[8]={'T','G','V','S','0','0','0','1'};
struct ParameterValue {juce::String id;float value=0;};
struct State {juce::MemoryBlock opaque;std::vector<ParameterValue> parameters;};
inline juce::MemoryBlock encodeState(const State& state)
{
    juce::MemoryOutputStream out;out.write(stateMagic,8);out.writeInt64((juce::int64)state.opaque.getSize());out.write(state.opaque.getData(),state.opaque.getSize());
    out.writeInt((int)state.parameters.size());
    for(const auto& p:state.parameters){const auto utf=p.id.toRawUTF8();const auto length=(int)std::strlen(utf);out.writeInt(length);out.write(utf,(size_t)length);out.writeFloat(p.value);}
    return out.getMemoryBlock();
}
inline bool isEnvelope(const juce::MemoryBlock& state)
{return state.getSize()>=8&&std::memcmp(state.getData(),stateMagic,8)==0;}
inline bool hasEnvelopePrefix(const juce::MemoryBlock& state)
{return state.getSize()>=4&&std::memcmp(state.getData(),stateMagic,4)==0;}
inline bool decodeState(const juce::MemoryBlock& bytes,State& result)
{
    using namespace juce;if(!isEnvelope(bytes))return false;
    MemoryInputStream in(bytes,false);in.setPosition(8);const auto length=in.readInt64();
    if(length<=0||length>45*1024*1024||length>in.getNumBytesRemaining()-4)return false;
    State candidate;candidate.opaque.setSize((size_t)length);if(in.read(candidate.opaque.getData(),(int)length)!=(int)length)return false;
    const int count=in.readInt();if(count<0||count>4096)return false;
    for(int index=0;index<count;++index)
    {
        if(in.getNumBytesRemaining()<8)return false;const int textLength=in.readInt();
        if(textLength<=0||textLength>256||textLength>in.getNumBytesRemaining()-4)return false;
        char text[257]{};if(in.read(text,textLength)!=textLength||std::memchr(text,0,(size_t)textLength))return false;
        ParameterValue value{String::fromUTF8(text,textLength),in.readFloat()};
        if(value.id.isEmpty()||!std::isfinite(value.value)||value.value<0||value.value>1)return false;
        for(const auto& old:candidate.parameters)if(old.id==value.id)return false;
        candidate.parameters.push_back(std::move(value));
    }
    if(in.getNumBytesRemaining()!=0)return false;result=std::move(candidate);return true;
}
inline void captureState(juce::AudioPluginInstance& instance,juce::MemoryBlock& output,bool extended)
{
    if(!extended){instance.getStateInformation(output);return;}
    State state;instance.getStateInformation(state.opaque);
    if(state.opaque.isEmpty())throw std::runtime_error("Plugin supplied empty state.");
    for(auto* parameter:instance.getParameters())if(parameter->isAutomatable())
    {
        const auto* hosted=dynamic_cast<juce::HostedAudioProcessorParameter*>(parameter);
        const auto id=hosted?hosted->getParameterID():juce::String();const auto value=parameter->getValue();
        if(id.isEmpty()||id.getNumBytesAsUTF8()>256||!std::isfinite(value)||value<0||value>1)throw std::runtime_error("Invalid VST3 automation state.");
        for(const auto& old:state.parameters)if(old.id==id)throw std::runtime_error("Duplicate VST3 parameter ID.");
        state.parameters.push_back({id,value});
    }
    if(state.parameters.size()>4096)throw std::runtime_error("Too many VST3 parameters.");output=encodeState(state);
}
inline void restoreState(juce::AudioPluginInstance& instance,const juce::MemoryBlock& input)
{
    if(!hasEnvelopePrefix(input)){instance.setStateInformation(input.getData(),(int)input.getSize());return;}
    State state;if(!decodeState(input,state))throw std::runtime_error("Corrupt extended VST3 state.");
    auto resolve=[&]
    {
        std::vector<juce::AudioProcessorParameter*> targets;
        for(const auto& value:state.parameters)
        {
            juce::AudioProcessorParameter* target=nullptr;
            for(auto* parameter:instance.getParameters())if(const auto* hosted=dynamic_cast<juce::HostedAudioProcessorParameter*>(parameter))
                if(hosted->getParameterID()==value.id&&parameter->isAutomatable()){if(target)throw std::runtime_error("Ambiguous VST3 parameter ID.");target=parameter;}
            if(!target)throw std::runtime_error("Saved VST3 parameter ID is unavailable.");targets.push_back(target);
        }
        return targets;
    };
    resolve(); // Reject missing/ambiguous IDs before touching the live plugin.
    instance.setStateInformation(state.opaque.getData(),(int)state.opaque.getSize());
    const auto targets=resolve(); // Do not retain parameter pointers across setState.
    for(size_t index=0;index<targets.size();++index)targets[index]->setValueNotifyingHost(state.parameters[index].value);
}
}
