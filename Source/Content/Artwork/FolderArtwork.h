#pragma once
#include <juce_graphics/juce_graphics.h>
#include <juce_core/juce_core.h>
namespace tonegrid::libraryui
{
// Shared NAM/IR convention: a local pack thumbnail wins, then shipped artwork,
// then its parent folder. Decoding/caching remain with each library worker.
template<class Decode,class Fallback>
juce::Image folderArtwork(juce::File folder,const juce::File& root,Decode&& decode,Fallback&& fallback)
{
    while(folder==root||folder.isAChildOf(root))
    {
        for(const auto* name:{"thumbnail.jpg","thumbnail.png","thumbnail.jpeg"})
            if(auto image=decode(folder.getChildFile(name));image.isValid())return image;
        if(auto image=fallback(folder);image.isValid())return image;
        if(folder==root)break;folder=folder.getParentDirectory();
    }
    return {};
}
}
