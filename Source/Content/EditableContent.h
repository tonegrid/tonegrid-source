#pragma once
#include "Platform/AppPaths.h"
namespace tonegrid::content {
// Immutable shipped defaults and explicit, writable user overrides.
struct FileLayer {
    juce::File shipped, user;
    juce::File effective() const { return user.existsAsFile() ? user : shipped; }
    juce::String revision() const {
        const auto file = effective();
        return file.getFullPathName() + ":" + juce::String(file.getSize()) + ":"
            + juce::String(file.getLastModificationTime().toMilliseconds());
    }
};
inline juce::Result createTemplate(const juce::File& destination, const juce::File& shipped) {
    if (destination.exists()) return juce::Result::ok();
    if (destination.getParentDirectory().createDirectory().failed())
        return juce::Result::fail("Cannot create the editable content folder.");
    juce::TemporaryFile staged(destination);
    if (!shipped.copyFileTo(staged.getFile()) || !staged.overwriteTargetFileWithTemporary())
        return juce::Result::fail("Cannot create content template: " + destination.getFileName());
    return juce::Result::ok();
}
inline juce::Result readText(const juce::File& file, juce::int64 limit, juce::String& text) {
    if (file.isSymbolicLink() || !file.existsAsFile() || file.getSize() <= 0 || file.getSize() > limit)
        return juce::Result::fail("Missing, linked or oversized content file: " + file.getFileName());
    text = file.loadFileAsString();
    return juce::Result::ok();
}
}
