#pragma once
#include "OutputDspParameters.h"
#include <juce_core/juce_core.h>
#include <cstdlib>
namespace tonegrid::output
{
inline juce::XmlElement encode(const Settings& s)
{
    juce::XmlElement xml("OutputDSP");xml.setAttribute("version",ampEq::hasDepth(s.ampEq)?7:ampEq::advanced(s.ampEq)?6:requiresAmpEq(s)?5:requiresExpanded(s)?4:s.reverb.type==reverb::Type::Spring?3:2);
    xml.setAttribute("ampNormalizationEnabled",s.ampNormalizationEnabled);
    if(s.ampNormalizationEnabled&&!s.ampNormalizationEnhanced)xml.setAttribute("ampNormalizationMode","partial");xml.setAttribute("spreadEnabled",s.spreadEnabled);
    xml.setAttribute("offsetMs",s.offsetMs);xml.setAttribute("wobblePercent",s.wobblePercent);xml.setAttribute("crossoverHz",s.crossoverHz);
    xml.setAttribute("wobbleEnabled",s.wobbleEnabled);xml.setAttribute("crossoverEnabled",s.crossoverEnabled);xml.setAttribute("diffuseEnabled",s.diffuseEnabled);
    auto* r=xml.createNewChildElement("Reverb");r->setAttribute("version",s.reverb.type==reverb::Type::Spring?2:1);r->setAttribute("enabled",s.reverb.enabled);r->setAttribute("type",int(s.reverb.type));
    r->setAttribute("timeSeconds",s.reverb.timeSeconds);r->setAttribute("preDelayMs",s.reverb.preDelayMs);r->setAttribute("color",s.reverb.color);r->setAttribute("mixPercent",s.reverb.mixPercent);
    if(requiresExpanded(s)||requiresAmpEq(s))
    {
        auto* p=xml.createNewChildElement("Pitch");p->setAttribute("version",1);p->setAttribute("mode",int(s.pitch.mode));p->setAttribute("semitones",s.pitch.semitones);p->setAttribute("dry",s.pitch.dry);p->setAttribute("down",s.pitch.down);p->setAttribute("up",s.pitch.up);
        auto* m=xml.createNewChildElement("Mod");m->setAttribute("version",1);m->setAttribute("enabled",s.mod.enabled);m->setAttribute("type",int(s.mod.type));m->setAttribute("rateHz",s.mod.rateHz);m->setAttribute("depth",s.mod.depth);m->setAttribute("level",s.mod.level);
        auto* d=xml.createNewChildElement("Delay");d->setAttribute("version",1);d->setAttribute("enabled",s.delay.enabled);d->setAttribute("timeMs",s.delay.timeMs);d->setAttribute("feedback",s.delay.feedback);d->setAttribute("mix",s.delay.mix);
    }
    if(requiresAmpEq(s)||ampEq::advanced(s.ampEq)){auto* eq=xml.createNewChildElement("AmpEq");eq->setAttribute("version",ampEq::hasDepth(s.ampEq)?3:ampEq::advanced(s.ampEq)?2:1);eq->setAttribute("bassDb",s.ampEq.bassDb);eq->setAttribute("midDb",s.ampEq.midDb);eq->setAttribute("trebleDb",s.ampEq.trebleDb);
        if(ampEq::advanced(s.ampEq)){
            eq->setAttribute("enabled",s.ampEq.enabled);eq->setAttribute("lowCut",s.ampEq.lowCut);eq->setAttribute("highCut",s.ampEq.highCut);
            eq->setAttribute("lowCutHz",s.ampEq.lowCutHz);eq->setAttribute("highCutHz",s.ampEq.highCutHz);
            eq->setAttribute("lowMidHz",s.ampEq.lowMidHz);eq->setAttribute("lowMidDb",s.ampEq.lowMidDb);eq->setAttribute("lowMidQ",s.ampEq.lowMidQ);
            eq->setAttribute("presenceHz",s.ampEq.presenceHz);eq->setAttribute("presenceDb",s.ampEq.presenceDb);eq->setAttribute("presenceQ",s.ampEq.presenceQ);
            if(ampEq::hasDepth(s.ampEq)){eq->setAttribute("depthDb",s.ampEq.depthDb);eq->setAttribute("depthHz",s.ampEq.depthHz);}
        }}
    return xml;
}
inline bool decode(const juce::XmlElement& xml,Settings& output)
{
    const auto version=xml.getStringAttribute("version");
    if(!xml.hasTagName("OutputDSP")||(version!="1"&&version!="2"&&version!="3"&&version!="4"&&version!="5"&&version!="6"&&version!="7"))return false;
    Settings s;
    const auto boolean=[&](const char* key,bool& value){const auto text=xml.getStringAttribute(key);if(text!="0"&&text!="1")return false;value=text=="1";return true;};
    const auto number=[&](const char* key,float& value){const auto text=xml.getStringAttribute(key).toStdString();if(text.empty())return false;char* end=nullptr;const auto parsed=std::strtod(text.c_str(),&end);if(end!=text.c_str()+text.size()||!std::isfinite(parsed))return false;value=(float)parsed;return true;};
    if(!boolean("ampNormalizationEnabled",s.ampNormalizationEnabled)||!boolean("spreadEnabled",s.spreadEnabled)
       ||!boolean("wobbleEnabled",s.wobbleEnabled)||!boolean("crossoverEnabled",s.crossoverEnabled)||!boolean("diffuseEnabled",s.diffuseEnabled)
       ||!number("offsetMs",s.offsetMs)||!number("wobblePercent",s.wobblePercent)||!number("crossoverHz",s.crossoverHz)||!valid(s))return false;
    if(xml.hasAttribute("ampNormalizationMode"))
    {
        const auto mode=xml.getStringAttribute("ampNormalizationMode");
        if(mode!="off"&&mode!="partial"&&mode!="full")return false;
        if(s.ampNormalizationEnabled!=(mode!="off"))return false;
        s.ampNormalizationEnhanced=mode!="partial";
    }
    int count=0;for(auto* child:xml.getChildIterator())count+=child->hasTagName("Reverb")?1:0;
    if(version=="1"){if(count!=0)return false;} // Historical rigs/preference: reverb OFF.
    else
    {
        auto* r=xml.getChildByName("Reverb");if(count!=1||!r||r->getStringAttribute("version")!=(version=="2"?"1":version=="3"?"2":r->getStringAttribute("type")=="3"?"2":"1"))return false;
        const auto flag=r->getStringAttribute("enabled"),type=r->getStringAttribute("type");
        if((flag!="0"&&flag!="1")||(type!="0"&&type!="1"&&type!="2"&&((version!="3"&&version!="4"&&version!="5"&&version!="6"&&version!="7")||type!="3")))return false;
        s.reverb.enabled=flag=="1";s.reverb.type=reverb::Type(type.getIntValue());
        const auto read=[&](const char* key,float& value){const auto text=r->getStringAttribute(key).toStdString();if(text.empty())return false;char* end=nullptr;const auto parsed=std::strtod(text.c_str(),&end);if(end!=text.c_str()+text.size()||!std::isfinite(parsed))return false;value=(float)parsed;return true;};
        if(!read("timeSeconds",s.reverb.timeSeconds)||!read("preDelayMs",s.reverb.preDelayMs)||!read("color",s.reverb.color)||!read("mixPercent",s.reverb.mixPercent)||!valid(s))return false;
    }
    for(const auto* tag:{"Pitch","Mod","Delay"})
    {
        int nodes=0;for(auto* child:xml.getChildIterator())nodes+=child->hasTagName(tag)?1:0;
        if(nodes!=((version=="4"||version=="5"||(version=="6"||version=="7"))?1:0))return false;
    }
    if(version=="4"||version=="5"||(version=="6"||version=="7"))
    {
        const auto* p=xml.getChildByName("Pitch");const auto* m=xml.getChildByName("Mod");const auto* d=xml.getChildByName("Delay");
        for(auto* node:{p,m,d})if(node->getStringAttribute("version")!="1")return false;
        const auto read=[](const juce::XmlElement& node,const char* key,float& value)
        {const auto text=node.getStringAttribute(key).toStdString();if(text.empty())return false;char* end=nullptr;const double v=std::strtod(text.c_str(),&end);if(end!=text.c_str()+text.size()||!std::isfinite(v))return false;value=float(v);return std::isfinite(value);};
        const auto booleanNode=[](const juce::XmlElement& node,const char* key,bool& value)
        {const auto text=node.getStringAttribute(key);if(text!="0"&&text!="1")return false;value=text=="1";return true;};
        float modeValue,typeValue,semitones;
        if(!read(*p,"mode",modeValue)||modeValue<0||modeValue>2||modeValue!=std::floor(modeValue)||!read(*p,"semitones",semitones)||semitones<-24||semitones>24||semitones!=std::floor(semitones))return false;
        if(!read(*m,"type",typeValue)||typeValue<0||typeValue>3||typeValue!=std::floor(typeValue))return false;
        s.pitch.mode=pitch::Mode(int(modeValue));s.pitch.semitones=int(semitones);s.mod.type=mod::Type(int(typeValue));
        if(!read(*p,"dry",s.pitch.dry)||!read(*p,"down",s.pitch.down)||!read(*p,"up",s.pitch.up)
            ||!booleanNode(*m,"enabled",s.mod.enabled)||!read(*m,"rateHz",s.mod.rateHz)||!read(*m,"depth",s.mod.depth)||!read(*m,"level",s.mod.level)
            ||!booleanNode(*d,"enabled",s.delay.enabled)||!read(*d,"timeMs",s.delay.timeMs)||!read(*d,"feedback",s.delay.feedback)||!read(*d,"mix",s.delay.mix)||!valid(s))return false;
        s.expandedEffects=true;
    }
    int eqCount=0;for(auto* child:xml.getChildIterator())eqCount+=child->hasTagName("AmpEq")?1:0;
    if(eqCount!=((version=="5"||(version=="6"||version=="7"))?1:0))return false;
    if(version=="5"||(version=="6"||version=="7"))
    {
        const auto* eq=xml.getChildByName("AmpEq");if(eq->getStringAttribute("version")!=(version=="7"?"3":version=="6"?"2":"1"))return false;
        const auto readEq=[&](const char* key,float& value){const auto text=eq->getStringAttribute(key).toStdString();if(text.empty())return false;char* end=nullptr;double v=std::strtod(text.c_str(),&end);if(end!=text.c_str()+text.size()||!std::isfinite(v))return false;value=float(v);return std::isfinite(value);};
        if(!readEq("bassDb",s.ampEq.bassDb)||!readEq("midDb",s.ampEq.midDb)||!readEq("trebleDb",s.ampEq.trebleDb)||!valid(s))return false;
        if((version=="6"||version=="7")){
            const auto flag=[&](const char* key,bool& value){const auto t=eq->getStringAttribute(key);if(t!="0"&&t!="1")return false;value=t=="1";return true;};
            if(!flag("enabled",s.ampEq.enabled)||!flag("lowCut",s.ampEq.lowCut)||!flag("highCut",s.ampEq.highCut)
                ||!readEq("lowCutHz",s.ampEq.lowCutHz)||!readEq("highCutHz",s.ampEq.highCutHz)
                ||!readEq("lowMidHz",s.ampEq.lowMidHz)||!readEq("lowMidDb",s.ampEq.lowMidDb)||!readEq("lowMidQ",s.ampEq.lowMidQ)
                ||!readEq("presenceHz",s.ampEq.presenceHz)||!readEq("presenceDb",s.ampEq.presenceDb)||!readEq("presenceQ",s.ampEq.presenceQ)||!valid(s))return false;
        }
        if(version=="7"&&(!readEq("depthDb",s.ampEq.depthDb)||!readEq("depthHz",s.ampEq.depthHz)||!valid(s)))return false;
        s.expandedAmpEq=true;
    }
    output=s;return true;
}
}
