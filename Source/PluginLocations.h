#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace vst3
{
inline juce::FileSearchPath defaultScanLocations()
{ return juce::VST3PluginFormat().getDefaultLocationsToSearch(); }
inline bool locationExists(const juce::String& location)
{
    // A foreign-platform or relative saved path is a hint, never a file in CWD.
    // Skip it safely before JUCE File construction; resolve via the local catalog.
    return juce::File::isAbsolutePath(location)&&juce::File(location).exists();
}
inline juce::File preferredTonexLocation(const juce::File& shared,const juce::File& user)
{ return !shared.exists()&&user.exists()?user:shared; }
inline juce::String defaultTonexPath()
{
#if JUCE_WINDOWS
    // Preserve the installed Windows default, including its legacy path hint.
    return "C:\\Program Files\\Common Files\\VST3\\TONEX.vst3";
#elif JUCE_MAC
    const auto shared=juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory).getChildFile("Audio/Plug-Ins/VST3/TONEX.vst3");
    const auto user=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Audio/Plug-Ins/VST3/TONEX.vst3");
    return preferredTonexLocation(shared,user).getFullPathName();
#else
    // TONEX has no native Linux build. Let the user select a native VST3.
    return {};
#endif
}
}
