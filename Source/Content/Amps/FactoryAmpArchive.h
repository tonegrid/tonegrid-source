#pragma once
#include "NamAmpState.h"
#include <json.hpp>
namespace namamp::factoryArchive
{
inline juce::File directory(const juce::File& library){return library.getSiblingFile("RetiredFullRigs");}
inline juce::String canonical(juce::String file){return file.startsWith("Factory/")?file.substring(8):file;}
// Exact former factory identity only. This never substitutes a Head for a Full Rig.
inline bool declares(const juce::File& library,const Identity& identity)
{
    if(!identity.valid())return false;
    const auto file=library.getChildFile("retired-full-rigs.json");
    if(!file.existsAsFile()||file.isSymbolicLink()||file.getSize()>2*1024*1024)return false;
    try
    {
        const auto json=nlohmann::json::parse(file.loadFileAsString().toStdString());
        if(json.value("schemaVersion",0)!=1||!json.at("captures").is_array()||json.at("captures").size()>4096)return false;
        for(const auto& entry:json.at("captures"))
            if(entry.at("file")==canonical(identity.file).toStdString()&&entry.at("sha256")==identity.sha256.toStdString()
               &&entry.at("bytes").get<juce::int64>()==identity.bytes)return true;
    }catch(...){}
    return false;
}
}
