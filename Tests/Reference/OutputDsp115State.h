// Frozen pre-Spring codec, 2026-10-01. Namespace/includes only changed.
#pragma once
#include "OutputDspParameters.h"
#include <juce_core/juce_core.h>
#include <cstdlib>
namespace tonegrid::reference::output
{
using tonegrid::output::Settings;using tonegrid::output::valid;
inline juce::XmlElement encode(const Settings& s)
{
    juce::XmlElement xml("OutputDSP");xml.setAttribute("version",2);
    xml.setAttribute("ampNormalizationEnabled",s.ampNormalizationEnabled);xml.setAttribute("spreadEnabled",s.spreadEnabled);
    xml.setAttribute("offsetMs",s.offsetMs);xml.setAttribute("wobblePercent",s.wobblePercent);xml.setAttribute("crossoverHz",s.crossoverHz);
    xml.setAttribute("wobbleEnabled",s.wobbleEnabled);xml.setAttribute("crossoverEnabled",s.crossoverEnabled);xml.setAttribute("diffuseEnabled",s.diffuseEnabled);
    auto* r=xml.createNewChildElement("Reverb");r->setAttribute("version",1);r->setAttribute("enabled",s.reverb.enabled);r->setAttribute("type",int(s.reverb.type));
    r->setAttribute("timeSeconds",s.reverb.timeSeconds);r->setAttribute("preDelayMs",s.reverb.preDelayMs);r->setAttribute("color",s.reverb.color);r->setAttribute("mixPercent",s.reverb.mixPercent);
    return xml;
}
inline bool decode(const juce::XmlElement& xml,Settings& output)
{
    const auto version=xml.getStringAttribute("version");
    if(!xml.hasTagName("OutputDSP")||(version!="1"&&version!="2"))return false;
    Settings s;
    const auto boolean=[&](const char* key,bool& value){const auto text=xml.getStringAttribute(key);if(text!="0"&&text!="1")return false;value=text=="1";return true;};
    const auto number=[&](const char* key,float& value){const auto text=xml.getStringAttribute(key).toStdString();if(text.empty())return false;char* end=nullptr;const auto parsed=std::strtod(text.c_str(),&end);if(end!=text.c_str()+text.size()||!std::isfinite(parsed))return false;value=(float)parsed;return true;};
    if(!boolean("ampNormalizationEnabled",s.ampNormalizationEnabled)||!boolean("spreadEnabled",s.spreadEnabled)
       ||!boolean("wobbleEnabled",s.wobbleEnabled)||!boolean("crossoverEnabled",s.crossoverEnabled)||!boolean("diffuseEnabled",s.diffuseEnabled)
       ||!number("offsetMs",s.offsetMs)||!number("wobblePercent",s.wobblePercent)||!number("crossoverHz",s.crossoverHz)||!valid(s))return false;
    int count=0;for(auto* child:xml.getChildIterator())count+=child->hasTagName("Reverb")?1:0;
    if(version=="1"){if(count!=0)return false;} // Historical rigs/preference: reverb OFF.
    else
    {
        auto* r=xml.getChildByName("Reverb");if(count!=1||!r||r->getStringAttribute("version")!="1")return false;
        const auto flag=r->getStringAttribute("enabled"),type=r->getStringAttribute("type");
        if((flag!="0"&&flag!="1")||(type!="0"&&type!="1"&&type!="2"))return false;
        s.reverb.enabled=flag=="1";s.reverb.type=reverb::Type(type.getIntValue());
        const auto read=[&](const char* key,float& value){const auto text=r->getStringAttribute(key).toStdString();if(text.empty())return false;char* end=nullptr;const auto parsed=std::strtod(text.c_str(),&end);if(end!=text.c_str()+text.size()||!std::isfinite(parsed))return false;value=(float)parsed;return true;};
        if(!read("timeSeconds",s.reverb.timeSeconds)||!read("preDelayMs",s.reverb.preDelayMs)||!read("color",s.reverb.color)||!read("mixPercent",s.reverb.mixPercent)||!valid(s))return false;
    }
    output=s;return true;
}
}
