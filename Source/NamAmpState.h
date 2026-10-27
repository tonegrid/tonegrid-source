#pragma once
#include "Content/Amps/WithdrawnAmpMigration.h"
#include "Content/Amps/NamMetadataMigrations.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <cmath>
#include <cstdlib>

namespace namamp
{
constexpr juce::int64 maxFileBytes=32*1024*1024;
inline bool validRelativePath(const juce::String& path)
{
    if(path.isEmpty()||path.length()>1024||path.startsWithChar('/')||path.containsAnyOf("\\:")||!path.endsWithIgnoreCase(".nam"))return false;
    const auto parts=juce::StringArray::fromTokens(path,"/",{});
    for(const auto& part:parts)if(part.isEmpty()||part=="."||part==".."||part.containsAnyOf("\r\n"))return false;
    return true;
}
inline juce::String filename(const juce::String& path){return path.fromLastOccurrenceOf("/",false,false);}
inline juce::String folderLabel(const juce::String& path)
{return path.containsChar('/')?path.upToLastOccurrenceOf("/",false,false).fromLastOccurrenceOf("/",false,false):"Root";}
struct Identity
{
    juce::String file,sha256;juce::int64 bytes=0;
    bool valid() const
    {
        return validRelativePath(file)
            &&bytes>0&&bytes<=maxFileBytes&&sha256.length()==64&&sha256.containsOnly("0123456789abcdef");
    }
    bool operator==(const Identity& other) const {return file==other.file&&bytes==other.bytes&&sha256==other.sha256;}
};
struct State {Identity model;float gainDb=0,volumeDb=0;};
inline bool validControls(float gain,float volume)
{return std::isfinite(gain)&&gain>=-24&&gain<=24&&std::isfinite(volume)&&volume>=-24&&volume<=24;}
inline bool valid(const State& state){return state.model.valid()&&validControls(state.gainDb,state.volumeDb);}
inline juce::XmlElement encode(const State& state)
{
    juce::XmlElement xml("AmpSource");xml.setAttribute("type","NAM");xml.setAttribute("file",state.model.file);
    xml.setAttribute("bytes",juce::String(state.model.bytes));xml.setAttribute("sha256",state.model.sha256);
    xml.setAttribute("gainDb",state.gainDb);xml.setAttribute("volumeDb",state.volumeDb);return xml;
}
inline bool decode(const juce::XmlElement& xml,State& output)
{
    if(!xml.hasTagName("AmpSource")||xml.getStringAttribute("type")!="NAM")return false;
    const auto bytes=xml.getStringAttribute("bytes");if(bytes.isEmpty()||!bytes.containsOnly("0123456789"))return false;
    State candidate;candidate.model={xml.getStringAttribute("file"),xml.getStringAttribute("sha256"),bytes.getLargeIntValue()};
    const auto number=[&](const char* key,float& value){const auto text=xml.getStringAttribute(key).toStdString();if(text.empty())return false;char* end=nullptr;const auto parsed=std::strtod(text.c_str(),&end);if(end!=text.c_str()+text.size()||!std::isfinite(parsed))return false;value=(float)parsed;return true;};
    if(!number("gainDb",candidate.gainDb)||!number("volumeDb",candidate.volumeDb))return false;
    if(!valid(candidate))return false;if(migrateWithdrawnAmp(candidate.model)){candidate.gainDb=0;candidate.volumeDb=0;}migrateNormalizationMetadata(candidate.model);output=std::move(candidate);return true;
}
}
