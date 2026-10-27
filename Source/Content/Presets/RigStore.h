#pragma once
#include "Platform/AppPaths.h"
#include "Content/Presets/RigCodec.h"
#include "Content/Presets/PresetTransaction.h"

namespace board
{
// Synchronous filesystem boundary, used on the message/test thread only.
// Explicit roots make storage tests independent of real user data. Construction
// performs no IO; migration is an explicit operation at the original startup point.
class RigStore
{
public:
    RigStore(File directory, File legacyDirectory)
        : bank(std::move(directory)), legacyBank(std::move(legacyDirectory)) {}

    static RigStore forApplicationData(const File& applicationData)
    {
        return {tonegrid::paths::userFolder(applicationData).getChildFile("Rigs-v1"),
                applicationData.getChildFile("ToneGrid/Legacy/Rigs-v3")};
    }

    const File& directory() const { return bank; }
    File slotFile(int slot) const
    { return bank.getChildFile("slot-" + String(slot + 1).paddedLeft('0', 3) + ".tonexrig"); }
    File nameFile(int slot) const
    { return bank.getChildFile("slot-" + String(slot + 1) + ".name"); }
    bool contains(int slot) const { return validSlot(slot) && slotFile(slot).existsAsFile(); }
    String readName(int slot) const
    {
        if (!contains(slot)) return {};
        const auto index = nameFile(slot), preset = slotFile(slot);
        if (index.existsAsFile() && index.getLastModificationTime() >= preset.getLastModificationTime())
            return index.loadFileAsString();
        std::unique_ptr<XmlElement> xml;
        return read(slot, xml).wasOk() ? xml->getStringAttribute("name") : String{};
    }
    static constexpr int maximumPresets = 10000;
    int capacity() const
    {
        int highest = rig::slotCount - 2;
        for (const auto& file : bank.findChildFiles(File::findFiles, false, "slot-*.tonexrig", File::FollowSymlinks::no))
        {
            const auto number = file.getFileNameWithoutExtension().substring(5);
            if (number.isNotEmpty() && number.containsOnly("0123456789"))
            {
                const int index = number.getIntValue() - 1;
                if (validSlot(index)) highest = juce::jmax(highest, index);
            }
        }
        // Keep one empty destination after the saved library, independently of MIDI.
        return juce::jlimit(rig::slotCount, maximumPresets, highest + 2);
    }

    Result recover() const{return tonegrid::storage::recover(bank);}
    Result rename(int slot,const String& newName) const
    {
        if(newName.trim().isEmpty())return Result::fail("Enter a preset name.");
        std::unique_ptr<XmlElement> xml;const auto loaded=read(slot,xml);if(loaded.failed())return loaded;
        if(!xml->hasTagName("PedalRig"))return Result::fail("Invalid rig XML.");
        xml->setAttribute("name",newName.trim());
        return tonegrid::storage::transact(bank,{tonegrid::storage::text(slotFile(slot),xml->toString()),tonegrid::storage::text(nameFile(slot),newName.trim())});
    }
    Result remove(int slot) const
    {
        if(!contains(slot))return Result::fail("Preset slot is empty.");
        return tonegrid::storage::transact(bank,{{slotFile(slot),{},false},{nameFile(slot),{},false}});
    }
    Result swap(int first,int second) const
    {
        if(!validSlot(first)||!validSlot(second)||std::abs(first-second)!=1||!contains(first))return Result::fail("Invalid preset move.");
        std::unique_ptr<XmlElement> a,b;auto result=read(first,a);if(result.failed())return result;
        if(contains(second)){result=read(second,b);if(result.failed())return result;}
        MemoryBlock firstBytes,secondBytes;
        if(!slotFile(first).loadFileAsData(firstBytes)||(b&&!slotFile(second).loadFileAsData(secondBytes)))return Result::fail("Cannot read presets for move.");
        // Preserve exact opaque plugin/model state bytes; only file positions change.
        std::vector<tonegrid::storage::Update> updates;
        updates.push_back({slotFile(first),std::move(secondBytes),bool(b)});
        updates.push_back(b?tonegrid::storage::text(nameFile(first),b->getStringAttribute("name")):tonegrid::storage::Update{nameFile(first),{},false});
        updates.push_back({slotFile(second),std::move(firstBytes),true});
        updates.push_back(tonegrid::storage::text(nameFile(second),a->getStringAttribute("name")));
        return tonegrid::storage::transact(bank,updates);
    }

    void migrateLegacyBank() const
    {
        // Preserve the one-shot, shallow copy, including its historical limits:
        // an existing destination is authoritative; failed copies are not retried.
        if (bank.exists()) return;
        if (bank.createDirectory().failed()) return;
        for (auto file : legacyBank.findChildFiles(File::findFiles, false))
            file.copyFileTo(bank.getChildFile(file.getFileName()));
    }

    struct SaveResult
    {
        Result result;
        bool nameIndexSaved = false;
    };

    SaveResult save(int slot, const State& state, const PluginDescription& description,
                    const RigIdentityLookup& identity) const
    {
        if(state.cabinet&&(state.sourceType!=amp::SourceType::Nam||!tonegrid::cabinet::valid(*state.cabinet)))return {Result::fail("Invalid cabinet IR state."),false};
        if (!validSlot(slot)) return {Result::fail("Invalid rig slot."), false};
        if(state.outputDsp&&!tonegrid::output::valid(*state.outputDsp))return {Result::fail("Invalid output DSP settings; save cancelled."),false};
        if(state.sourceType!=amp::SourceType::Vst3&&state.sourceType!=amp::SourceType::Nam)return {Result::fail("Invalid amp source type."),false};
        if(state.sourceType==amp::SourceType::Nam&&!namamp::valid(state.namAmp))return {Result::fail("Invalid NAM amp state; save cancelled."),false};
        if(state.sourceType==amp::SourceType::Vst3&&(state.genericVst3||state.outputDsp.has_value())&&!vst3::Identity::of(description).valid())return {Result::fail("Invalid VST3 rig identity; save cancelled."),false};
        if (state.sourceType==amp::SourceType::Vst3&&(state.amp.isEmpty() || state.amp.getSize() > 45 * 1024 * 1024))
            return {Result::fail(state.genericVst3?"Invalid VST3 state; save cancelled.":"Invalid TONEX state; save cancelled."), false};
        const auto written=tonegrid::storage::transact(bank,{tonegrid::storage::text(slotFile(slot),encode(state,description,identity).toString()),tonegrid::storage::text(nameFile(slot),state.name)});
        return {written,written.wasOk()};
    }

    Result readSnapshot(int slot,std::unique_ptr<XmlElement>& xml) const{return read(slot,xml);}
    Result load(int slot, const PluginDescription& expected,
                const RigIdentityLookup& identity, State& result) const
    {
        std::unique_ptr<XmlElement> xml;auto loaded=read(slot,xml);
        return loaded.failed()?loaded:decode(*xml,expected,identity,result);
    }

    // Full candidate validation before resolving/instantiating another amp. The
    // application must then compare this saved description with live metadata.
    Result loadForSelection(int slot,const RigIdentityLookup& identity,State& result,PluginDescription& description) const
    {
        std::unique_ptr<XmlElement> xml;auto loaded=read(slot,xml);if(loaded.failed())return loaded;
        PluginDescription saved;auto* plugin=xml->getChildByName("PLUGIN");
        const auto* source=xml->getChildByName("AmpSource");const auto version=xml->getIntAttribute("version");
        const bool native=version==8||(version>=9&&source&&source->getStringAttribute("type")=="NAM");
        if(!native&&(!plugin||!saved.loadFromXml(*plugin)))return Result::fail("Invalid rig plugin description.");
        State candidate;auto decoded=decode(*xml,saved,identity,candidate);if(decoded.failed())return decoded;
        result=std::move(candidate);description=std::move(saved);return Result::ok();
    }

private:
    Result read(int slot,std::unique_ptr<XmlElement>& xml) const
    {
        if (!validSlot(slot)) return Result::fail("Invalid rig slot.");
        const auto file = slotFile(slot);
        if (file.getSize() <= 0 || file.getSize() > 128 * 1024 * 1024)
            return Result::fail("Rig missing or oversized.");
        xml = XmlDocument::parse(file);
        if (!xml) return Result::fail("Invalid rig XML.");
        return Result::ok();
    }

    static bool validSlot(int slot) { return slot >= 0 && slot < maximumPresets; }
    const File bank, legacyBank;
};
}
