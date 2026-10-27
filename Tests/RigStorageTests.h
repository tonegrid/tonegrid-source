#pragma once
#include "Content/Presets/RigStore.h"
#if JUCE_WINDOWS
#include <windows.h>
#endif

namespace storageTests
{
using namespace juce;

inline bool sameState(const board::State& a, const board::State& b)
{
    if (a.name != b.name || a.amp != b.amp || a.input.gainDb != b.input.gainDb
        || a.input.thresholdDb != b.input.thresholdDb || a.input.gate != b.input.gate) return false;
    for (int p = 0; p < board::count; ++p)
    {
        const auto& x = a.pedals[(size_t)p]; const auto& y = b.pedals[(size_t)p];
        if (x.preset != y.preset || x.enabled != y.enabled || x.input != y.input
            || x.mix != y.mix || x.output != y.output || x.ratio != y.ratio || x.tone != y.tone
            || x.wahSensitivity != y.wahSensitivity || x.wahRange != y.wahRange
            || x.wahDecay != y.wahDecay || x.eqBands != y.eqBands) return false;
    }
    return true;
}

struct TemporaryDirectory
{
    const File parent = File::getSpecialLocation(File::tempDirectory);
    const File root = parent.getChildFile("ToneGrid-storage-tests-" + Uuid().toString());
    ~TemporaryDirectory() { if (root.isAChildOf(parent)) root.deleteRecursively(); }
};

inline bool run(const File& fixture, String& report)
{
    bool ok = true; int checks = 0;
    auto check = [&](bool value, const String& label)
    { ++checks; ok &= value; report += String(value ? "PASS " : "FAIL ") + label + "\n"; };
    TemporaryDirectory temporary;
    check(temporary.root.createDirectory().wasOk(), "Storage sandbox created under system temp");
    if (!ok) return false;
    const auto fixtureXml = XmlDocument::parse(fixture);
    if (!fixtureXml || !fixtureXml->getChildByName("Rigs"))
    { check(false, "Storage frozen fixtures available"); return false; }
    // Preserve the on-disk frozen fixture. Storage/codec tests use an in-memory
    // copy with retired NAM indices brought within the new bank's range; the
    // separate compatibility suite verifies actual old capture rejection.
    auto syntheticRigs=std::make_unique<XmlElement>(*fixtureXml->getChildByName("Rigs"));
    for(auto* rig:syntheticRigs->getChildIterator())for(auto* item:rig->getChildIterator())
    {
        const int p=item->getIntAttribute("id",-1),n=item->getIntAttribute("preset",-1);
        if(p>=0&&p<board::count&&board::isNam(p)&&n>=board::presetCount(p))
        {
            item->setAttribute("preset",0);
            const auto* bank=fixtureXml->getChildByAttribute("id",String(p));
            const auto* entry=bank?bank->getChildByAttribute("index","0"):nullptr;
            if(entry)item->setAttribute("model",entry->getStringAttribute("identity"));
        }
    }
    const auto* rigs = syntheticRigs.get();
    const auto* version6 = rigs->getChildByAttribute("version", "6");
    if (!version6) { check(false, "Storage v6 fixture available"); return false; }
    PluginDescription plugin;
    plugin.loadFromXml(*version6->getChildByName("PLUGIN"));
    // Use frozen metadata, not a live processor, to exercise the pure codec/store.
    const board::RigIdentityLookup identity = [&](int p, int preset)
    {
        const auto* definition = fixtureXml->getChildByAttribute("id", String(p));
        const auto* entry = definition ? definition->getChildByAttribute("index", String(preset)) : nullptr;
        return entry ? entry->getStringAttribute("identity") : String();
    };
    const auto appData = temporary.root.getChildFile("synthetic-appdata");
    const auto store = board::RigStore::forApplicationData(appData);
    check(!appData.exists(), "Constructing storage has no filesystem side effects");
    check(store.directory() == appData.getChildFile("ToneGrid/Rigs-v1"), "Current user-data relative path preserved");
    check(store.slotFile(0).getFileName() == "slot-001.tonexrig"
        && store.slotFile(127).getFileName() == "slot-128.tonexrig", "Padded first/last rig filenames");
    check(store.nameFile(0).getFileName() == "slot-1.name"
        && store.nameFile(127).getFileName() == "slot-128.name", "Unpadded historical name filenames");

    board::State original;
    check(board::decode(*version6, plugin, identity, original).wasOk(), "Synthetic rig decoded without live engine");
    if (!ok) return false;
    auto expanded = original;
    expanded.outputDsp = tonegrid::output::Settings{};
    expanded.outputDsp->expandedEffects = true;
    expanded.outputDsp->pitch = {tonegrid::pitch::Mode::Octaver, -7, 74, 41, 22};
    expanded.outputDsp->mod = {true, tonegrid::mod::Type::Flanger, 3.2f, 63, 44};
    expanded.outputDsp->delay = {true, 789.5f, 71, 29};
    auto expandedXml = board::encode(expanded, plugin, identity);
    board::State expandedRecall; PluginDescription selected;
    check(expandedXml.getIntAttribute("version") == 12 && store.save(20, expanded, plugin, identity).result.wasOk()
        && store.loadForSelection(20, identity, expandedRecall, selected).wasOk()
        && expandedRecall.outputDsp && tonegrid::output::encode(*expandedRecall.outputDsp).isEquivalentTo(expandedXml.getChildByName("OutputDSP"), false)
        && sameState(expanded, expandedRecall), "Disk rig v12 round trip retains every new effect, opaque amp and stable pedal IDs");
    const auto renamedExpanded=std::make_unique<XmlElement>(expandedXml);renamedExpanded->setAttribute("name","Renamed complete preset");
    check(store.rename(20,"Renamed complete preset").wasOk()&&store.readName(20)=="Renamed complete preset"&&XmlDocument::parse(store.slotFile(20))->isEquivalentTo(renamedExpanded.get(),false),"Rename preserves every XML field and opaque amp byte while updating both names");
    const auto renameBefore=store.slotFile(20).loadFileAsString();
    check(store.rename(20," ").failed()&&store.rename(-1,"X").failed()&&store.rename(40,"X").failed()&&store.slotFile(20).loadFileAsString()==renameBefore,"Blank, invalid and empty rename targets cannot change files");
    check(store.nameFile(20).deleteFile()&&store.nameFile(20).createDirectory().wasOk(),"Prepare blocked rename sidecar");
    check(store.rename(20,"Should fail").failed()&&store.slotFile(20).loadFileAsString()==renameBefore,"Failed name-index rename restores original complete XML");
    check(store.remove(20).failed()&&store.slotFile(20).loadFileAsString()==renameBefore,"Delete rejects blocked sidecar without losing preset");
    store.nameFile(20).deleteFile();store.nameFile(20).replaceWithText("Renamed complete preset");
    check(store.remove(20).wasOk()&&!store.contains(20)&&!store.nameFile(20).exists()&&store.remove(20).failed()&&store.remove(128).failed(),"Delete removes preset and index, validates slot bounds and refuses empty slots");
    store.save(20,expanded,plugin,identity);
    auto invalidEffect = expanded; invalidEffect.outputDsp->delay.feedback = 100;
    const auto safeDisk = store.slotFile(20).loadFileAsString();
    check(store.save(20, invalidEffect, plugin, identity).result.failed() && store.slotFile(20).loadFileAsString() == safeDisk,
        "Invalid new effect state cannot replace a recoverable saved rig");
    for (int version : {9, 10, 11})
    {
        auto old = original; old.outputDsp = tonegrid::output::Settings{};
        if (version == 11) old.outputDsp->reverb.type = tonegrid::reverb::Type::Spring;
        auto oldXml = board::encode(old, plugin, identity);
        if (version == 9) { oldXml.setAttribute("version", 9); auto* dsp = oldXml.getChildByName("OutputDSP"); dsp->setAttribute("version", 1); dsp->removeChildElement(dsp->getChildByName("Reverb"), true); }
        rig::writeXml(oldXml, store.slotFile(21));
        const auto bytes = store.slotFile(21).loadFileAsString(); const auto modified = store.slotFile(21).getLastModificationTime();
        board::State recalledOld;
        check(store.loadForSelection(21, identity, recalledOld, selected).wasOk() && recalledOld.outputDsp
            && recalledOld.outputDsp->pitch.mode == tonegrid::pitch::Mode::Off && !recalledOld.outputDsp->mod.enabled && !recalledOld.outputDsp->delay.enabled
            && store.slotFile(21).loadFileAsString() == bytes && store.slotFile(21).getLastModificationTime() == modified,
            "Rig v" + String(version) + " loads all new effects OFF without rewriting bytes or timestamp");
    }
    auto saved = store.save(0, original, plugin, identity);
    check(saved.result.wasOk() && saved.nameIndexSaved && store.contains(0), "Save creates bank, XML and name index");
    auto disk = XmlDocument::parse(store.slotFile(0));
    auto legacyDisk=disk?std::make_unique<XmlElement>(*disk):nullptr;
    if(legacyDisk)legacyDisk->removeAttribute("presetId");
    auto legacyFormat=std::make_unique<XmlElement>(*version6);legacyFormat->removeAttribute("presetId");
    check(disk&&disk->getStringAttribute("presetId").isNotEmpty()&&legacyDisk->isEquivalentTo(legacyFormat.get(), false), "Saved XML retains frozen sound settings and adds its stable preset identity");
    board::State recalled;
    check(store.load(0, plugin, identity, recalled).wasOk() && sameState(original, recalled), "Disk round trip preserves full state and opaque TONEX bytes");
    check(store.readName(0) == original.name, "Name index round trip");
    auto renamed = original; renamed.name = "  Renamed synthetic rig  "; renamed.pedals[1].tone = 1.25f;
    saved = store.save(0, renamed, plugin, identity);
    check(saved.result.wasOk() && saved.nameIndexSaved && store.load(0, plugin, identity, recalled).wasOk()
        && sameState(renamed, recalled) && store.readName(0) == renamed.name, "Replace existing slot and preserve raw name whitespace");
    check(store.save(127, original, plugin, identity).result.wasOk()
        && store.load(127, plugin, identity, recalled).wasOk() && sameState(original, recalled), "Last slot 128 round trips");

    for (auto* xml : rigs->getChildIterator())
    {
        const int version = xml->getIntAttribute("version");
        board::State expected, actual;
        const bool written = rig::writeXml(*xml, store.slotFile(version));
        const bool decoded = board::decode(*xml, plugin, identity, expected).wasOk();
        check(written && decoded && store.load(version, plugin, identity, actual).wasOk()
            && sameState(expected, actual), "Frozen v" + String(version) + " loads from disk without migration writeback");
        const auto unchanged = XmlDocument::parse(store.slotFile(version));
        check(unchanged && unchanged->isEquivalentTo(xml, false), "Reading v" + String(version) + " leaves source XML untouched");
        const auto migratedOutput=actual.outputDsp.value_or(tonegrid::output::legacySettings());
        check(migratedOutput.pitch.mode==tonegrid::pitch::Mode::Off&&!migratedOutput.mod.enabled&&!migratedOutput.delay.enabled,
            "Legacy rig v"+String(version)+" keeps all new effects OFF");
        if (version < 6)
        {
            bool defaultsKept = true;
            for (int p = 7; p < 13; ++p)
                defaultsKept &= !actual.pedals[(size_t)p].enabled && actual.pedals[(size_t)p].preset == board::recommendedPreset(p);
            check(defaultsKept, "Historical seven-pedal format supplies disabled new pedals v" + String(version));
        }
        if (version < 5)
            check(actual.pedals[1].input == 12 && actual.pedals[1].mix == 1 && actual.pedals[1].tone == 0,
                  "Legacy NAM trim/mix/tone migration v" + String(version));
        if (version == 3)
            check(actual.input.gainDb == 0 && actual.input.thresholdDb == -62 && !actual.input.gate,
                  "Version 3 missing Input keeps historical defaults");
    }

    const auto sentinel = renamed;
    auto rejectedLoad = [&](int slot, const PluginDescription& description,
                            const board::RigIdentityLookup& lookup, const String& error, const String& label)
    {
        auto destination = sentinel;
        const auto result = store.load(slot, description, lookup, destination);
        check(result.failed() && result.getErrorMessage() == error && sameState(destination, sentinel), label);
    };
    rejectedLoad(100, plugin, identity, "Rig missing or oversized.", "Missing rig rejected without mutation");
    check(store.slotFile(100).replaceWithText(""), "Create empty synthetic rig");
    rejectedLoad(100, plugin, identity, "Rig missing or oversized.", "Empty rig rejected without mutation");
    check(store.slotFile(100).replaceWithText("This is not XML."), "Create malformed synthetic XML");
    rejectedLoad(100, plugin, identity, "Invalid rig XML.", "Malformed XML rejected without mutation");
    {
        FileOutputStream oversized(store.slotFile(100));
        check(oversized.openedOk() && oversized.setPosition(128LL * 1024 * 1024)
            && oversized.writeByte(0), "Create oversized synthetic rig");
    }
    rejectedLoad(100, plugin, identity, "Rig missing or oversized.", "128 MiB file limit enforced before XML parsing");
    check(store.slotFile(100).deleteFile(), "Remove oversized synthetic rig");
    rejectedLoad(-1, plugin, identity, "Invalid rig slot.", "Negative slot rejected without mutation");
    rejectedLoad(board::RigStore::maximumPresets, plugin, identity, "Invalid rig slot.", "Out-of-range expanded slot rejected without mutation");
    check(!store.contains(-1) && store.readName(board::RigStore::maximumPresets).isEmpty()
        && store.save(board::RigStore::maximumPresets, original, plugin, identity).result.failed(), "Invalid storage indices cannot create slots beyond the expanded bound");

    auto rejectXml = [&](XmlElement xml, const String& error, const String& label)
    {
        check(rig::writeXml(xml, store.slotFile(100)), "Write synthetic rejection case: " + label);
        rejectedLoad(100, plugin, identity, error, label);
    };
    auto xml = *version6; xml.setAttribute("version", 18);
    rejectXml(xml, "A version 3 through 17 pedalboard rig is required.", "Unknown format rejected");
    xml = *version6; xml.getChildByName("Amp")->setAttribute("sha256", "broken");
    rejectXml(xml, "Missing or corrupt TONEX state.", "Corrupt TONEX hash rejected");
    xml = *version6; xml.getChildByName("Amp")->setAttribute("bytes", 999);
    rejectXml(xml, "Missing or corrupt TONEX state.", "TONEX byte count mismatch rejected");
    xml = *version6; xml.removeChildElement(xml.getChildByName("Input"), true);
    rejectXml(xml, "Missing input/gate settings.", "Modern rig requires Input settings");
    xml = *version6; xml.getChildByName("Input")->setAttribute("gain", 99);
    rejectXml(xml, "Invalid input/gate settings.", "Out-of-range input rejected");
    xml = *version6; xml.addChildElement(new XmlElement(*xml.getChildByName("Pedal")));
    rejectXml(xml, "Invalid pedal list.", "Duplicate persistent pedal ID rejected");
    xml = *version6; xml.removeChildElement(xml.getChildByName("Pedal"), true);
    rejectXml(xml, "Incomplete pedalboard.", "Missing pedal rejected");
    xml = *version6; xml.getChildByAttribute("id", "1")->setAttribute("model", "changed");
    rejectXml(xml, "Invalid settings or changed capture for TS9", "Changed capture identity rejected");
    xml = *version6; xml.getChildByAttribute("id", "1")->setAttribute("tone", 7);
    rejectXml(xml, "Invalid settings or changed capture for TS9", "Out-of-range NAM tone rejected");
    xml = *version6; xml.getChildByAttribute("id", "1")->setAttribute("preset", -1);
    rejectXml(xml, "Invalid settings or changed capture for TS9", "Invalid preset rejected before identity lookup");
    xml = *rigs->getChildByAttribute("version", "3"); xml.getChildByAttribute("id", "1")->setAttribute("mix", 2);
    rejectXml(xml, "Invalid legacy NAM settings.", "Invalid legacy mix rejected before migration");
    auto otherPlugin = plugin; otherPlugin.version = "different";
    check(store.load(0,otherPlugin,identity,recalled).wasOk(),"Updated TONEX version accepted with unchanged plugin identity");
    otherPlugin = plugin; otherPlugin.fileOrIdentifier = "other-location.vst3";
    rejectedLoad(0, otherPlugin, identity, "TONEX identity differs from this saved rig.", "TONEX path identity equality preserved");
    const board::RigIdentityLookup changedIdentity = [](int, int) { return String("changed"); };
    rejectedLoad(0, plugin, changedIdentity, "Invalid settings or changed capture for COMP", "Supplied catalog identity remains authoritative");
    xml = *version6; xml.getChildByAttribute("id", "9")->setAttribute("model", "auto-wah-v1");
    check(rig::writeXml(xml, store.slotFile(100)) && store.load(100, plugin, identity, recalled).wasOk()
        && sameState(original, recalled), "Legacy Wah identity accepted without changing settings");

    check(store.nameFile(0).deleteFile() && store.contains(0) && store.readName(0)==renamed.name
        && store.load(0, plugin, identity, recalled).wasOk() && sameState(renamed, recalled)&&!store.nameFile(0).exists(), "Missing name index uses the XML title without changing the saved preset or synthesizing files");
    check(store.nameFile(0).replaceWithText("stale name") && store.load(0, plugin, identity, recalled).wasOk()
        && recalled.name == renamed.name && store.readName(0) == "stale name", "XML name authoritative on recall; stale index kept for slot listing");
    check(store.nameFile(0).deleteFile() && store.nameFile(0).createDirectory().wasOk(), "Simulate name index write failure");
    saved = store.save(0, original, plugin, identity);
    check(saved.result.failed() && !saved.nameIndexSaved && store.load(0, plugin, identity, recalled).wasOk()
        && sameState(renamed, recalled), "Name failure preserves both original files without partial success");

    auto emptyAmp = original; emptyAmp.amp.reset();
    const auto oldBytes = store.slotFile(0).loadFileAsString();
    saved = store.save(0, emptyAmp, plugin, identity);
    check(saved.result.getErrorMessage() == "Invalid TONEX state; save cancelled."
        && store.slotFile(0).loadFileAsString() == oldBytes, "Empty TONEX state cannot replace saved rig");
    emptyAmp.amp.setSize(45 * 1024 * 1024 + 1, true);
    saved = store.save(0, emptyAmp, plugin, identity);
    check(saved.result.getErrorMessage() == "Invalid TONEX state; save cancelled."
        && store.slotFile(0).loadFileAsString() == oldBytes, "45 MiB TONEX save limit preserves existing rig");
    emptyAmp.amp.reset();

    // Deny deletion of an existing rig: TemporaryFile replacement must fail and
    // keep both the original XML and its name sidecar intact.
    check(store.nameFile(127).replaceWithText("keep-index"), "Seed name index for failed replacement");
    const auto previousXml = store.slotFile(127).loadFileAsString();
#if JUCE_WINDOWS
    HANDLE locked = CreateFileW(store.slotFile(127).getFullPathName().toWideCharPointer(), GENERIC_READ,
                                FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    check(locked != INVALID_HANDLE_VALUE, "Lock synthetic rig against replacement");
    if (locked != INVALID_HANDLE_VALUE)
    {
        saved = store.save(127, renamed, plugin, identity);
        CloseHandle(locked);
        check(saved.result.failed() && !saved.nameIndexSaved
            && store.slotFile(127).loadFileAsString() == previousXml && store.readName(127) == "keep-index",
              "Failed XML replacement preserves rig and does not write name index");
    }
#else
    // POSIX permits renaming open files. Deny writes to the synthetic bank
    // instead, and verify that a failed save still preserves both old files.
    check(store.directory().setReadOnly(true), "Make synthetic bank read-only");
    saved = store.save(127, renamed, plugin, identity);
    check(store.directory().setReadOnly(false), "Restore synthetic bank permissions");
    check(saved.result.failed() && !saved.nameIndexSaved
        && store.slotFile(127).loadFileAsString() == previousXml && store.readName(127) == "keep-index",
          "Failed XML save preserves rig and does not write name index");
#endif
    const auto blocked = temporary.root.getChildFile("not-a-directory");
    check(blocked.replaceWithText("sentinel"), "Create blocked synthetic bank parent");
    const board::RigStore blockedStore(blocked.getChildFile("bank"), temporary.root.getChildFile("absent-legacy"));
    check(blockedStore.save(0, original, plugin, identity).result.failed()
        && blocked.loadFileAsString() == "sentinel", "Failed directory creation preserves blocking file");
    blockedStore.migrateLegacyBank();
    check(!blockedStore.directory().exists() && blocked.loadFileAsString() == "sentinel", "Failed migration destination creation is non-destructive");

    const auto migrationAppData = temporary.root.getChildFile("migration-appdata");
    const auto legacy = migrationAppData.getChildFile("ToneGrid/Legacy/Rigs-v3");
    const auto migrated = board::RigStore::forApplicationData(migrationAppData);
    check(legacy.createDirectory().wasOk() && version6->writeTo(legacy.getChildFile("slot-001.tonexrig"))
        && legacy.getChildFile("slot-1.name").replaceWithText(original.name)
        && legacy.getChildFile("midi.xml").replaceWithText("synthetic MIDI bytes")
        && legacy.getChildFile("extra.txt").replaceWithText("copy all files"), "Seed synthetic historical bank");
    check(legacy.getChildFile("nested").createDirectory().wasOk()
        && legacy.getChildFile("nested/ignored.txt").replaceWithText("not recursive"), "Seed nested legacy directory");
    migrated.migrateLegacyBank();
    check(migrated.load(0, plugin, identity, recalled).wasOk() && sameState(original, recalled)
        && migrated.readName(0) == original.name, "First-run bank migration copies rig and name");
    check(migrated.directory().getChildFile("midi.xml").loadFileAsString() == "synthetic MIDI bytes"
        && migrated.directory().getChildFile("extra.txt").loadFileAsString() == "copy all files"
        && !migrated.directory().getChildFile("nested").exists(), "Historical migration remains shallow and copies all file types");
    check(legacy.getChildFile("slot-001.tonexrig").existsAsFile(), "Migration leaves legacy source in place");
    check(migrated.nameFile(0).replaceWithText("destination wins")
        && legacy.getChildFile("late.txt").replaceWithText("do not merge"), "Prepare repeated migration");
    migrated.migrateLegacyBank();
    check(migrated.readName(0) == "destination wins" && !migrated.directory().getChildFile("late.txt").exists(), "Existing destination never overwritten or merged");
    check(migrated.slotFile(0).deleteFile(), "Simulate partially copied existing destination");
    migrated.migrateLegacyBank();
    check(!migrated.contains(0) && legacy.getChildFile("slot-001.tonexrig").existsAsFile(), "Historical partial migration is not automatically retried");
    const auto emptyBank = board::RigStore::forApplicationData(temporary.root.getChildFile("no-legacy"));
    emptyBank.migrateLegacyBank();
    check(emptyBank.directory().isDirectory() && !emptyBank.contains(0), "Absent legacy bank creates empty destination as before");
    check(temporary.root.deleteRecursively() && !temporary.root.exists(), "All synthetic storage files removed");
    report += String(checks) + " storage checks; " + (ok ? "ALL PASSED" : "FAILURES") + "\n";
    return ok;
}
}
