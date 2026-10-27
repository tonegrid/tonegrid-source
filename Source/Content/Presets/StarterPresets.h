#pragma once
#include "Content/Presets/RigStore.h"
#include "Content/Amps/NamAmpLibrary.h"
namespace tonegrid::starters
{
struct SeedResult{juce::Result result=juce::Result::ok();bool installed=false;int startupSlot=0;};
inline bool hasPresets(const board::RigStore& store)
{for(int i=0;i<store.capacity();++i)if(store.contains(i))return true;return false;}
inline SeedResult seed(const juce::File& source,const board::RigStore& store,const namamp::Catalog& catalog,const board::RigIdentityLookup& lookup)
{
    using namespace juce;SeedResult out;const bool existing=hasPresets(store);
    try
    {
        const auto manifest=source.getChildFile("manifest.json");if(!manifest.existsAsFile()||manifest.getSize()>64*1024)return {Result::fail("Starter preset manifest is missing or too large.")};
        const auto hash=SHA256(manifest).toHexString();const auto receipt=store.directory().getChildFile("starter-presets-sha256.txt");
        if(receipt.loadFileAsString().trim()==hash)return out;
        const auto doc=nlohmann::json::parse(manifest.loadFileAsString().toStdString());const auto& presets=doc.at("presets");
        if(doc.at("version")!=1||!presets.is_array()||presets.empty()||presets.size()>16)return {Result::fail("Invalid starter preset inventory.")};
        out.startupSlot=doc.at("startupSlot").get<int>();std::set<int> slots;std::vector<storage::Update> updates;
        for(const auto& item:presets)
        {
            const int slot=item.at("slot").get<int>();const String name(item.at("file").get<std::string>()),hash(item.at("sha256").get<std::string>());
            if(slot<0||slot>=rig::slotCount||!slots.insert(slot).second||name!=store.slotFile(slot).getFileName())return {Result::fail("Invalid starter preset slot.")};
            const auto file=source.getChildFile(name);if(file.isSymbolicLink()||!file.existsAsFile()||file.getSize()>1024*1024||SHA256(file).toHexString()!=hash)return {Result::fail("Starter preset missing or altered: "+name)};
            const auto xml=XmlDocument::parse(file);board::State state;PluginDescription none;
            if(!xml)return {Result::fail("Invalid starter preset XML.")};const auto decoded=board::decode(*xml,none,lookup,state);if(decoded.failed())return {decoded};
            if(state.sourceType!=amp::SourceType::Nam||!state.outputDsp||!state.outputDsp->ampNormalizationEnabled||!state.outputDsp->ampNormalizationEnhanced)return {Result::fail("Starter preset must use a NAM amp with Full normalization.")};
            const namamp::Entry* match=nullptr;for(const auto& entry:catalog.entries)if(entry.identity.sha256==state.namAmp.model.sha256){match=&entry;if(entry.identity.file==state.namAmp.model.file)break;}
            if(!match)return {Result::fail("Starter preset amp missing: "+state.namAmp.model.file)};state.namAmp.model=match->identity;
            if(store.slotFile(slot).exists()||store.nameFile(slot).exists())continue;
            updates.push_back(storage::text(store.slotFile(slot),board::encode(state,none,lookup).toString()));updates.push_back(storage::text(store.nameFile(slot),state.name));
        }
        if(!slots.contains(out.startupSlot))return {Result::fail("Starter launch preset is not in the inventory.")};
        const bool added=!updates.empty();updates.push_back(storage::text(receipt,hash));
        out.result=storage::transact(store.directory(),updates);out.installed=added&&out.result.wasOk();if(existing)out.startupSlot=-1;return out;
    }
    catch(const std::exception& error){return {Result::fail("Starter presets: "+String(error.what()))};}
}
}
