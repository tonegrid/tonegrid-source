#pragma once
#include "Vst3Identity.h"
#include "PluginLocations.h"
#include "RigSupport.h"
#include <vector>

namespace vst3
{
struct ScanFailure {juce::String location,reason;};
struct Catalog
{
    std::vector<juce::PluginDescription> plugins;
    std::vector<ScanFailure> failures;
    void add(const juce::PluginDescription& d)
    {
        if(!Identity::of(d).valid())return;
        for(auto& old:plugins)if(Identity::of(old)==Identity::of(d)&&old.fileOrIdentifier==d.fileOrIdentifier){old=d;return;}
        plugins.push_back(d);
    }
    void sort()
    {std::sort(plugins.begin(),plugins.end(),[](const auto& a,const auto& b){const auto order=label(a).compareIgnoreCase(label(b));return order!=0?order<0:a.fileOrIdentifier<b.fileOrIdentifier;});}
    juce::XmlElement xml() const
    {
        juce::XmlElement result("ToneGridVst3Cache");result.setAttribute("version",1);
        for(const auto& d:plugins)result.addChildElement(d.createXml().release());
        for(const auto& failure:failures){auto* item=result.createNewChildElement("Failure");item->setAttribute("location",failure.location);item->setAttribute("reason",failure.reason);}
        return result;
    }
    static juce::Result decode(const juce::XmlElement& xml,Catalog& output)
    {
        using namespace juce;
        if(!xml.hasTagName("ToneGridVst3Cache")||xml.getIntAttribute("version")!=1||xml.getNumChildElements()>4096)return Result::fail("Invalid VST3 cache.");
        Catalog candidate;
        for(auto* child:xml.getChildIterator())
        {
            if(child->hasTagName("PLUGIN"))
            {PluginDescription d;if(!d.loadFromXml(*child)||!Identity::of(d).valid()||d.fileOrIdentifier.isEmpty())return Result::fail("Invalid cached VST3 identity.");candidate.add(d);}
            else if(child->hasTagName("Failure"))candidate.failures.push_back({child->getStringAttribute("location"),child->getStringAttribute("reason")});
            else return Result::fail("Invalid VST3 cache entry.");
        }
        candidate.sort();output=std::move(candidate);return Result::ok();
    }
    // Hint wins only among identity matches. A stale/moved hint resolves after rescan.
    // The loader always revalidates the chosen file before touching the active amp.
    juce::Result resolve(const juce::PluginDescription& wanted,juce::PluginDescription& output) const
    {
        using namespace juce;
        if(!Identity::of(wanted).valid())return Result::fail("Invalid requested VST3 identity.");
        if(locationExists(wanted.fileOrIdentifier)){output=wanted;return Result::ok();}
        std::vector<const PluginDescription*> matches;
        for(const auto& d:plugins)if(Identity::of(d)==Identity::of(wanted)&&locationExists(d.fileOrIdentifier))matches.push_back(&d);
        if(matches.size()==1){output=*matches[0];return Result::ok();}
        return Result::fail(matches.empty()?"VST3 plugin missing: "+label(wanted)+". Install it or Rescan VST3.":"Ambiguous VST3 identity: "+label(wanted)+". Restore its original location.");
    }
};
class CacheStore
{
public:
    explicit CacheStore(juce::File file):cache(std::move(file)){}
    juce::Result read(Catalog& result) const
    {
        if(!cache.existsAsFile())return juce::Result::fail("No VST3 cache. Click Scan VST3.");
        if(cache.getSize()<=0||cache.getSize()>8*1024*1024)return juce::Result::fail("VST3 cache missing or oversized.");
        auto xml=juce::XmlDocument::parse(cache);
        return xml?Catalog::decode(*xml,result):juce::Result::fail("Invalid VST3 cache XML. Click Rescan VST3.");
    }
    bool write(const Catalog& next) const {return rig::writeXml(next.xml(),cache);}
    const juce::File& file() const {return cache;}
private:juce::File cache;
};
}
