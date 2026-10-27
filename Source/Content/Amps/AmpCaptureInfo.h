#pragma once
#include "Content/Amps/NamLibraryIndex.h"

namespace tonegrid::libraryui
{
// Verified public pack metadata, keyed by NAM content rather than filename or
// folder. Renaming a capture preserves its provenance without changing its title.
class AmpCaptureInfo final
{
public:
    explicit AmpCaptureInfo(const juce::File& factory)
    {
        if(factory==juce::File{})return;
        const auto file=factory.getChildFile("capture-info.json");
        if(!file.existsAsFile()||file.isSymbolicLink()||file.getSize()>4*1024*1024)return;
        try
        {
            const auto json=nlohmann::json::parse(file.loadFileAsString().toStdString());
            if(json.value("schemaVersion",0)!=1||!json.contains("packs")||!json["packs"].is_object()
                ||!json.contains("captures")||!json["captures"].is_object())return;
            for(auto it=json["captures"].begin();it!=json["captures"].end();++it)
            {
                const juce::String hash(it.key());
                if(hash.length()!=64||!hash.containsOnly("0123456789abcdef")||!it.value().is_string())continue;
                const auto pack=json["packs"].find(it.value().get<std::string>());
                if(pack==json["packs"].end()||!pack->is_object())continue;
                Capture c;c.make=known(text(*pack,"maker"));c.model=known(text(*pack,"model"));
                c.description=known(text(*pack,"description"));c.author=known(text(*pack,"author"));
                c.gear=text(*pack,"gear");
                if(auto types=json.find("captureGear");types!=json.end()&&types->is_object())
                    if(auto type=types->find(it.key());type!=types->end()&&type->is_string())c.gear=juce::String(type->get<std::string>());
                c.creatorUsername=text(*pack,"creator_username");c.sourceUrl=tonePage(text(*pack,"url"));
                if(c.sourceUrl.isNotEmpty())entries.emplace(hash,std::move(c));
            }
        }
        catch(...){entries.clear();}
    }
    void apply(Capture& c,const juce::String& hash)const
    {
        const auto it=entries.find(hash);if(it==entries.end())return;const auto& info=it->second;
        if(info.gear.isNotEmpty())c.gear=info.gear;c.make=info.make;c.model=info.model;c.description=info.description;c.sourceUrl=info.sourceUrl;
        if(info.author.isNotEmpty())c.author=info.author;c.creatorUsername=info.creatorUsername;
    }
private:std::map<juce::String,Capture> entries;
};
}
