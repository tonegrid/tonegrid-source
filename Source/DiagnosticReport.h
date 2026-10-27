#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <ToneGridBuildInfo.h>

namespace tonegrid::diagnostics
{
inline constexpr const char* recipient="christian@tonegrid.net";
inline constexpr const char* build=tonegrid::build::sourceRevision;
inline juce::String sanitize(const juce::String& value,juce::StringArray privateRoots={})
{
    if(privateRoots.isEmpty())privateRoots.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory).getFullPathName());
    juce::StringArray lines;lines.addLines(value);juce::StringArray result;
    for(auto line:lines)
    {
        const auto lower=line.toLowerCase();
        if(lower.contains("token")||lower.contains("bearer")||lower.contains("authorization")||lower.contains("api_key")||lower.contains("api-key")||lower.contains("code="))
            line="[Authentication detail omitted]";
        else for(const auto& root:privateRoots)if(root.isNotEmpty())
        {line=line.replace(root,"[User folder]",true).replace(root.replaceCharacter('\\','/'),"[User folder]",true);}
        result.add(line);
    }
    return result.joinIntoString("\n");
}
inline juce::String recentLog(const juce::String& text)
{
    juce::StringArray lines;lines.addLines(text);while(lines.size()>32)lines.remove(0);
    auto tail=lines.joinIntoString("\n");if(tail.length()>6000)tail="[Earlier log details omitted]\n"+tail.getLastCharacters(6000);
    return sanitize(tail);
}
inline juce::String subject(){return "ToneGrid "+juce::String(JUCE_APPLICATION_VERSION_STRING)+" diagnostic";}
inline juce::String mailto(const juce::String& body)
{return juce::String("mailto:")+recipient+"?subject="+juce::URL::addEscapeChars(subject(),true)+"&body="+juce::URL::addEscapeChars(body.replace("\n","\r\n"),true);}
inline juce::String systemSummary()
{
    using namespace juce;String text="ToneGrid diagnostic\nVersion: "+String(JUCE_APPLICATION_VERSION_STRING)+"\nBuild: "+String(build)+"\nTime: "+Time::getCurrentTime().toISO8601(true)+"\n";
    text+="OS: "+SystemStats::getOperatingSystemName()+"\nApplication: 64-bit\nCPU: "+SystemStats::getCpuVendor()+" "+SystemStats::getCpuModel()+"\nRAM: "+String(SystemStats::getMemorySizeInMegabytes())+" MB\n";
    text+="JUCE: "+SystemStats::getJUCEVersion()+"\n";
    for(const auto& display:Desktop::getInstance().getDisplays().displays)
        text+="Display: "+display.userArea.toString()+"; scale "+String(display.scale,2)+"\n";
    return text;
}
}
