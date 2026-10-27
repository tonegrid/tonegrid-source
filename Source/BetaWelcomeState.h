#pragma once
#include <juce_core/juce_core.h>
namespace tonegrid::welcome
{
inline bool dismissed(const juce::File& file)
{
    if(!file.existsAsFile()||file.getSize()>4096)return false;
    const auto xml=juce::XmlDocument::parse(file);
    return xml&&xml->hasTagName("BetaWelcome")&&xml->getStringAttribute("version")=="1"&&xml->getStringAttribute("dismissed")=="1";
}
inline juce::Result dismiss(const juce::File& file)
{
    if(file.getParentDirectory().createDirectory().failed())return juce::Result::fail("Cannot save welcome message preference.");
    juce::XmlElement xml("BetaWelcome");xml.setAttribute("version",1);xml.setAttribute("dismissed",1);
    juce::TemporaryFile temporary(file);
    if(!xml.writeTo(temporary.getFile())||!temporary.overwriteTargetFileWithTemporary())return juce::Result::fail("Cannot save welcome message preference. It will appear again next time.");
    return juce::Result::ok();
}
}
