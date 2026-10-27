#pragma once
#include "Platform/AppPaths.h"
#include <juce_graphics/juce_graphics.h>

namespace tonegrid::assets
{
// Artwork is loaded once at startup. A failed edit falls back to the shipped
// image, then the embedded image; it cannot affect DSP or capture loading.
struct SkinFolders137 {juce::File user,bundled;};
inline SkinFolders137& skinFolders137()
{static SkinFolders137 folders{{},paths::installed().resources().getChildFile("PedalSkins")};return folders;}
inline void configureSkinFolders137(const paths::Layout& layout)
{skinFolders137()={layout.user().getChildFile("PedalSkins"),layout.resources().getChildFile("PedalSkins")};}

inline juce::Image readSkinPng137(const juce::File& file,juce::Point<int> expected)
{
    if(!file.existsAsFile()||file.getSize()<24||file.getSize()>32*1024*1024)return {};
    auto stream=file.createInputStream();if(!stream)return {};
    unsigned char header[24]{};constexpr unsigned char signature[]{137,80,78,71,13,10,26,10};
    if(stream->read(header,24)!=24||std::memcmp(header,signature,8)!=0
       ||juce::ByteOrder::bigEndianInt(header+16)!=juce::uint32(expected.x)
       ||juce::ByteOrder::bigEndianInt(header+20)!=juce::uint32(expected.y))return {};
    stream->setPosition(0);auto image=juce::PNGImageFormat().decodeImage(*stream);
    return image.isValid()&&image.getWidth()==expected.x&&image.getHeight()==expected.y?image:juce::Image{};
}
inline juce::Image loadSkinPng137(const SkinFolders137& folders,const juce::String& filename,juce::Point<int> expected,const juce::Image& fallback)
{
    for(const auto& root:{folders.user,folders.bundled})if(root!=juce::File{})
        if(auto image=readSkinPng137(root.getChildFile(filename),expected);image.isValid())return image;
    return fallback;
}
}
