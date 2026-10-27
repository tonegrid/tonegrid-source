#pragma once
#include <juce_core/juce_core.h>
#include <vector>
namespace tonegrid::media {
struct Track {juce::String id,title,creator,style,key,mode,url,suggestedScale;double bpm=0;};
inline juce::String videoId(const juce::String& address)
{juce::URL url(address);if(url.getDomain()=="youtu.be")return url.getSubPath();const auto index=url.getParameterNames().indexOf("v");return index>=0?url.getParameterValues()[index]:juce::String{};}
inline bool safeTrackUrl(const juce::String& address)
{
    juce::URL url(address);const auto host=url.getDomain().toLowerCase();
    if(!address.startsWith("https://")||(host!="www.youtube.com"&&host!="youtube.com"&&host!="youtu.be"))return false;
    juce::String id;
    if(host=="youtu.be")id=url.getSubPath();
    else {if(url.getSubPath()!="watch")return false;const auto index=url.getParameterNames().indexOf("v");if(index>=0)id=url.getParameterValues()[index];}
    return id.length()==11&&id.containsOnly("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-");
}
}
