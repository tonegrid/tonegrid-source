#pragma once
#include "Content/Amps/NamAmpLibrary.h"
#include "Content/Amps/AmpCaptureInfo.h"
#include "CaptureKind.h"
namespace tonegrid::captures {
// One policy for live fallback and offline rendering. It returns a domain value,
// never an Image, UI component, neural model or mutable audio state.
struct ResolvedKind {
    juce::Result result = juce::Result::ok();
    cabinet::CaptureKind kind = cabinet::CaptureKind::unknown;
};
inline ResolvedKind resolveKind(const namamp::Library& library, const namamp::Identity& identity,
                                const juce::File& factory = {}) {
    namamp::Entry entry; nlohmann::json payload;
    const auto result = library.resolve(identity, entry, payload);
    if (result.failed()) return {result, cabinet::CaptureKind::unknown};
    libraryui::Capture info;
    libraryui::embedded(info, payload);
    const auto root = library.folder();
    const auto file = root.getChildFile(entry.identity.file);
    libraryui::sidecar(info, file, entry.identity.sha256);
    const libraryui::AmpCaptureInfo metadata(factory == juce::File{} ? root : factory);
    metadata.apply(info, entry.identity.sha256);
    const auto overrides = root.getChildFile(".tonegrid-types.json");
    if (overrides.existsAsFile() && !overrides.isSymbolicLink() && overrides.getSize() < 128 * 1024) {
        try {
            const auto values = nlohmann::json::parse(overrides.loadFileAsString().toStdString());
            if (auto it=values.find(entry.identity.sha256.toStdString());it!=values.end()&&it->is_string())
                info.gear=juce::String(it->get<std::string>());
        } catch (const std::exception&) { /* A malformed optional override does not erase provenance. */ }
    }
    if (namamp::factoryArchive::declares(root, identity)) info.gear="amp_cab";
    return {juce::Result::ok(), cabinet::kind(info.gear)};
}
}
