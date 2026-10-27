#pragma once
#include "LevelCalibration.h"
#include "Content/EditableContent.h"
#include <ToneGridApplicationAssets.h>
#include <set>
#include <vector>
namespace tonegrid::calibration {
struct InterfaceProfile {
    juce::String id, name, settings, source, notes;
    float reference = 0;
    std::vector<int> channels;
    juce::StringArray aliases;
    juce::String inputInstructions, gainInstructions, gainConfirmation;
    bool estimated = false;
    bool matchesId(const juce::String& value) const { return id == value || aliases.contains(value); }
    bool supports(int channel) const { return std::find(channels.begin(), channels.end(), channel) != channels.end(); }
};
inline juce::Result decodeInterfaceProfiles(const juce::String& text, std::vector<InterfaceProfile>& out) {
    try {
        if (text.getNumBytesAsUTF8() > 2 * 1024 * 1024) throw std::runtime_error("Catalogue exceeds 2 MB.");
        const auto json = nlohmann::json::parse(text.toStdString());
        const auto& rows = json.at("profiles");
        if (json.at("schemaVersion") != 1 || !rows.is_array() || rows.empty() || rows.size() > 2048)
            throw std::runtime_error("Expected schemaVersion 1 and 1 to 2,048 profiles.");
        std::vector<InterfaceProfile> next;
        std::set<juce::String> ids;
        for (const auto& value : rows) {
            InterfaceProfile p;
            p.id = value.at("id").get<std::string>();
            p.name = value.at("manufacturer").get<std::string>() + " " + value.at("model").get<std::string>() + " (" + value.at("generation").get<std::string>() + ")";
            p.reference = value.at("referenceDbU").get<float>();
            p.settings = value.at("settings").get<std::string>();
            p.source = value.at("source").get<std::string>();
            p.notes = value.at("notes").get<std::string>();
            p.channels = value.at("instrumentChannels").get<std::vector<int>>();
            if (p.id.isEmpty() || p.id.length() > 120 || !ids.insert(p.id).second || p.name.length() > 400
                || !valid({true, p.reference}) || p.channels.empty() || p.channels.size() > 64
                || !p.source.startsWith("https://") || p.settings.length() > 8000 || p.notes.length() > 8000)
                throw std::runtime_error("Invalid or duplicate profile: " + p.id.toStdString());
            std::set<int> channels;
            for (int channel : p.channels)
                if (channel < 0 || channel > 63 || !channels.insert(channel).second)
                    throw std::runtime_error("Invalid instrumentChannels: " + p.id.toStdString());
            if (value.contains("aliases")) for (const auto& alias : value.at("aliases")) {
                const juce::String id(alias.get<std::string>());
                if (id.isEmpty() || id.length() > 120 || !ids.insert(id).second) throw std::runtime_error("Invalid or duplicate profile alias.");
                p.aliases.add(id);
            }
            p.inputInstructions = value.value("inputInstructions", std::string{});
            p.gainInstructions = value.value("gainInstructions", std::string{});
            p.gainConfirmation = value.value("gainConfirmation", std::string{});
            p.estimated = value.value("estimated", false);
            next.push_back(std::move(p));
        }
        out = std::move(next);
        return juce::Result::ok();
    } catch (const std::exception& error) {
        return juce::Result::fail("Interface profiles: " + juce::String(error.what()));
    }
}
inline std::vector<InterfaceProfile> interfaceProfiles(const juce::File& overrideFile = {}, juce::String* warning = nullptr) {
    std::vector<InterfaceProfile> result;
    decodeInterfaceProfiles(juce::String::fromUTF8(ToneGridApplicationAssets::interfaceprofiles_json,
        ToneGridApplicationAssets::interfaceprofiles_jsonSize), result);
    if (overrideFile.exists()) {
        juce::String text;
        auto loaded = content::readText(overrideFile, 2 * 1024 * 1024, text);
        if (loaded.wasOk()) loaded = decodeInterfaceProfiles(text, result);
        if (loaded.failed() && warning) *warning = loaded.getErrorMessage() + " Bundled profiles kept.";
    }
    return result;
}
}
