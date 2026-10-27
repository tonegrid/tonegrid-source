#include "Tone3000.h"
#include <filesystem>
#include <fstream>
#if JUCE_WINDOWS
#include <windows.h>
#include <bcrypt.h>
#endif
namespace tonegrid::t3k
{
String entropy(size_t count)
{
    MemoryBlock bytes(count);
#if JUCE_WINDOWS
    if(BCryptGenRandom(nullptr,static_cast<PUCHAR>(bytes.getData()),static_cast<ULONG>(count),BCRYPT_USE_SYSTEM_PREFERRED_RNG)!=0)return {};
#else
    std::ifstream random("/dev/urandom",std::ios::binary);if(!random.read(static_cast<char*>(bytes.getData()),static_cast<std::streamsize>(count)))return {};
#endif
    return base64url(bytes.getData(),bytes.getSize());
}
bool commitPack(const File& from,const File& to)
{
    // JUCE File::moveFileTo deletes an existing destination first.
#if JUCE_WINDOWS
    return MoveFileW(from.getFullPathName().toWideCharPointer(),to.getFullPathName().toWideCharPointer())!=0;
#else
    if(to.exists())return false;std::error_code error;
    std::filesystem::rename(std::filesystem::u8path(from.getFullPathName().toStdString()),std::filesystem::u8path(to.getFullPathName().toStdString()),error);return !error;
#endif
}
}
