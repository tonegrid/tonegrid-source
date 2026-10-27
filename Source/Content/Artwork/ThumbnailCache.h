#pragma once
#include "Tone3000.h"
namespace tonegrid
{
class ThumbnailCache final
{
public:
    struct Value {juce::Image image;juce::var metadata;};
    const Value& get(const juce::File& model)
    {
        using namespace juce;const auto manifest=model.getSiblingFile(t3k::manifestName);
        auto stamp=[](const File& f){return f.getFullPathName()+":"+String(f.getSize())+":"+String(f.getLastModificationTime().toMilliseconds());};
        const auto manifestKey=stamp(manifest)+model.getFileName();
        if(manifestKey!=metadataKey){metadataKey=manifestKey;cached.metadata=t3k::localManifest(model);}
        String key=manifestKey+model.getFileName();std::vector<File> images;
        for(const auto* extension:{".png",".jpg",".jpeg"})images.push_back(model.withFileExtension(extension));
        const auto name=t3k::textField(cached.metadata,"image");
        if(name.isNotEmpty()&&!name.containsAnyOf("/\\:")&&name!="."&&name!="..")images.push_back(model.getSiblingFile(name));
        for(const auto* name:{"thumbnail.jpg","thumbnail.png","thumbnail.jpeg"})images.push_back(model.getSiblingFile(name));
        for(const auto& file:images)key+=stamp(file);
        if(key!=imageKey)
        {
            imageKey=key;cached.image={};for(const auto& file:images)if(file.existsAsFile()&&t3k::noLinks(file)){auto image=t3k::boundedImage(file);if(image.isValid()){const double scale=std::min(1.0,1024.0/std::max(image.getWidth(),image.getHeight()));cached.image=SoftwareImageType().convert(image).rescaled(juce::jmax(1,juce::roundToInt(image.getWidth()*scale)),juce::jmax(1,juce::roundToInt(image.getHeight()*scale)),Graphics::highResamplingQuality);break;}}
        }
        return cached;
    }
private:juce::String metadataKey,imageKey;Value cached;
};
}
