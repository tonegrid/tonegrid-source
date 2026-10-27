#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace vst3
{
// JUCE's two class-ID hashes, qualified by format/vendor. Location/name are hints.
// Keep the historical pair for strict restore compatibility. JUCE normalises
// uniqueId; deprecatedUid hashes native bytes and may differ across systems.
// Cross-system equivalence must be verified with real plugins before relaxing it.
struct Identity
{
    juce::String format,manufacturer;
    int uniqueId=0,deprecatedUid=0;
    bool valid() const {return format=="VST3"&&(uniqueId!=0||deprecatedUid!=0);}
    bool operator==(const Identity& other) const
    {return format==other.format&&manufacturer==other.manufacturer&&uniqueId==other.uniqueId&&deprecatedUid==other.deprecatedUid;}
    static Identity of(const juce::PluginDescription& d)
    {return {d.pluginFormatName,d.manufacturerName,d.uniqueId,d.deprecatedUid};}
    juce::XmlElement xml() const
    {
        juce::XmlElement result("AmpSource");result.setAttribute("type","VST3");result.setAttribute("format",format);
        result.setAttribute("manufacturer",manufacturer);result.setAttribute("uniqueId",uniqueId);result.setAttribute("deprecatedUid",deprecatedUid);return result;
    }
    static bool read(const juce::XmlElement& xml,Identity& output)
    {
        if(!xml.hasTagName("AmpSource")||xml.getStringAttribute("type")!="VST3"
            ||!xml.hasAttribute("uniqueId")||!xml.hasAttribute("deprecatedUid"))return false;
        Identity candidate{xml.getStringAttribute("format"),xml.getStringAttribute("manufacturer"),xml.getIntAttribute("uniqueId"),xml.getIntAttribute("deprecatedUid")};
        if(!candidate.valid())return false;output=std::move(candidate);return true;
    }
};
inline juce::String label(const juce::PluginDescription& d)
{return d.manufacturerName.isEmpty()?d.name:d.manufacturerName+" — "+d.name;}
}
