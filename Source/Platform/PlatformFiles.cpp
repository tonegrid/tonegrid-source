#include "Platform/PlatformFiles.h"
#include <filesystem>
#if JUCE_WINDOWS
#include <windows.h>
#endif

namespace tonegrid::platform
{
bool commitExport(const juce::File& staged,const juce::File& destination)
{
#if JUCE_WINDOWS
    return MoveFileExW(staged.getFullPathName().toWideCharPointer(),destination.getFullPathName().toWideCharPointer(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    std::error_code error;
    std::filesystem::rename(std::filesystem::u8path(staged.getFullPathName().toStdString()),
                            std::filesystem::u8path(destination.getFullPathName().toStdString()),error);
    return !error;
#endif
}
juce::String missingEncoderMessage()
{
#if JUCE_WINDOWS
    return "The MP3 encoder is missing. Keep the Tools folder beside ToneGrid.exe.";
#else
    return "The MP3 encoder is missing. Reinstall ToneGrid.";
#endif
}
}
