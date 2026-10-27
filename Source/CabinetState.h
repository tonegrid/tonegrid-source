#pragma once
#include <juce_core/juce_core.h>
#include <cmath>

namespace tonegrid::cabinet
{
inline bool safeRelative(const juce::String& name)
{
    if(name.isEmpty()||name.length()>1024||name.startsWithChar('/')||name.containsAnyOf("\\:\r\n"))return false;
    for(const auto& part:juce::StringArray::fromTokens(name,"/",{}))if(part.isEmpty()||part=="."||part=="..")return false;
    return true;
}
struct State
{
    juce::String file,sha256;
    bool locked=false;
    bool bypassed=false;
    bool empty()const{return file.isEmpty()&&sha256.isEmpty();}
};
inline bool valid(const State& s)
{return s.empty()||(safeRelative(s.file)&&s.file.endsWithIgnoreCase(".wav")&&s.sha256.length()==64&&s.sha256.containsOnly("0123456789abcdef"));}
inline juce::XmlElement encode(const State& s)
{juce::XmlElement xml("CabinetIR");xml.setAttribute("version",1);xml.setAttribute("file",s.file);xml.setAttribute("sha256",s.sha256);xml.setAttribute("locked",s.locked);xml.setAttribute("bypassed",s.bypassed);return xml;}
inline bool decode(const juce::XmlElement& xml,State& out)
{
    State s;s.file=xml.getStringAttribute("file");s.sha256=xml.getStringAttribute("sha256");
    const auto locked=xml.getStringAttribute("locked");
    if(!xml.hasTagName("CabinetIR")||xml.getStringAttribute("version")!="1"||(locked!="0"&&locked!="1")||!valid(s))return false;
    const auto bypassed=xml.getStringAttribute("bypassed","0");if(bypassed!="0"&&bypassed!="1")return false;
    s.locked=locked=="1";s.bypassed=bypassed=="1";out=std::move(s);return true;
}
}
