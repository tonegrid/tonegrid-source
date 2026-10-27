#pragma once
#include <juce_core/juce_core.h>

namespace tonegrid::platform
{
// Called on the export worker. Both files are staged in the destination folder;
// publish only a completed export, preserving the old file when publication fails.
bool commitExport(const juce::File& staged,const juce::File& destination);
juce::String missingEncoderMessage();
}
