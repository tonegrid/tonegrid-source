#pragma once
#include <juce_core/juce_core.h>
namespace tonegrid::storage {
inline juce::Result writeXmlAtomically(const juce::File& file, const juce::XmlElement& xml) {
    if (file.isSymbolicLink()) return juce::Result::fail("Linked preference files cannot be replaced.");
    const auto directory=file.getParentDirectory().createDirectory();
    if(directory.failed())return directory;
    juce::TemporaryFile temporary(file);
    if(!xml.writeTo(temporary.getFile())||!temporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail("Cannot save "+file.getFileName()+". Previous preferences kept.");
    return juce::Result::ok();
}
}
