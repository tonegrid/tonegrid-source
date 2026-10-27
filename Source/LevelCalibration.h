#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "NamCalibrationMetadata.h"
#include <cstdlib>
#include <limits>
#include <array>

namespace tonegrid::calibration
{
// Audio-thread-owned routing for this graph block. Both processors run serially
// on the same callback. Recording per chunk avoids using the LAST pedal state
// for the ENTIRE block when a bypass/capture transition occurs mid-block.
struct ReferenceRouting
{
    static constexpr int chunk=128;
    std::array<bool,1024> known{};
    int chunks=0;
    bool at(int sample) const noexcept
    {const int index=sample/chunk;return index>=0&&index<chunks&&index<int(known.size())&&known[size_t(index)];}
};
// dBu of a full-scale sine at the instrument ADC. Missing is explicit.
struct Settings
{
    bool enabled=true;
    float referenceDbU=std::numeric_limits<float>::quiet_NaN();
    bool completed=false,needsVerification=false;
    bool signalVerified=false; // Informational only; never changes DSP gains.
    juce::String profileId="manual",deviceType,deviceName;
    int channel=-1;
    Settings()=default;
    Settings(bool on,float reference):enabled(on),referenceDbU(reference),completed(true){}
};
inline bool valid(const Settings& s) noexcept
{return std::isfinite(s.referenceDbU)&&s.referenceDbU>=-20.f&&s.referenceDbU<=40.f;}
inline bool active(const Settings& s) noexcept
{return s.enabled&&valid(s)&&s.completed&&!s.needsVerification;}
inline bool needsOnboarding(const Settings& s) noexcept
{return !s.completed||!valid(s);}
inline std::optional<float> parseReference(const juce::String& text)
{
    const auto trimmed=text.trim();if(trimmed.isEmpty()||!trimmed.containsOnly("+-0123456789.eE"))return {};
    const auto encoded=trimmed.toStdString();char* end=nullptr;const double value=std::strtod(encoded.c_str(),&end);
    if(end!=encoded.c_str()+encoded.size()||!std::isfinite(value)||value< -20||value>40)return {};
    return float(value);
}
inline std::optional<double> inputLevel(nam::DSP& m) noexcept{return metadata(m).input;}
inline std::optional<double> outputLevel(nam::DSP& m) noexcept{return metadata(m).output;}
inline float inputGain(float reference,std::optional<double> level) noexcept
{return std::isfinite(reference)&&level&&validLevel(*level)?juce::Decibels::decibelsToGain(float(reference-*level)):1.f;}
inline float outputGain(float reference,std::optional<double> level) noexcept
{return std::isfinite(reference)&&level&&validLevel(*level)?juce::Decibels::decibelsToGain(float(*level-reference)):1.f;}
struct PedalGains{float input=1.f,output=1.f;};
inline PedalGains pedalGains(const Settings& s,std::optional<double> input,std::optional<double> output) noexcept
{
    if(!active(s))return {};
    return {inputGain(s.referenceDbU,input),outputGain(s.referenceDbU,output)};
}
inline bool matches(const Settings& s,const juce::String& type,const juce::String& name,int channel) noexcept
{return s.completed&&s.deviceType==type&&s.deviceName==name&&s.channel==channel&&channel>=0&&name.isNotEmpty();}
inline Settings read(const juce::File& file)
{
    if(file.getSize()>8192)return {};
    const auto xml=juce::XmlDocument::parse(file);
    if(!xml||!xml->hasTagName("LevelCalibration"))return {};
    const int version=xml->getIntAttribute("version");if(version!=1&&version!=2)return {};
    const auto flag=xml->getStringAttribute("enabled");if(flag!="0"&&flag!="1")return {};
    const auto reference=parseReference(xml->getStringAttribute("referenceDbU"));if(!reference)return {};
    Settings s;s.enabled=flag=="1";s.referenceDbU=*reference;
    // V1 has no physical-device confirmation; require setup, preserving preference.
    if(version==2)
    {
        s.profileId=xml->getStringAttribute("profileId","manual");s.deviceType=xml->getStringAttribute("deviceType");
        s.deviceName=xml->getStringAttribute("deviceName");s.channel=xml->getIntAttribute("channel",-1);
        s.completed=xml->getBoolAttribute("completed")&&s.channel>=0&&s.channel<256&&s.deviceName.isNotEmpty()&&s.deviceType.isNotEmpty();
        s.signalVerified=s.completed&&xml->getBoolAttribute("signalVerified",false);
    }
    return s;
}
inline bool write(const juce::File& file,const Settings& s)
{
    if(!valid(s)||file.getParentDirectory().createDirectory().failed())return false;
    juce::XmlElement xml("LevelCalibration");xml.setAttribute("version",2);
    xml.setAttribute("enabled",s.enabled?1:0);xml.setAttribute("referenceDbU",double(s.referenceDbU));
    xml.setAttribute("profileId",s.profileId);xml.setAttribute("deviceType",s.deviceType);xml.setAttribute("deviceName",s.deviceName);
    xml.setAttribute("channel",s.channel);xml.setAttribute("completed",s.completed);
    xml.setAttribute("signalVerified",s.signalVerified);
    juce::TemporaryFile temporary(file);
    return temporary.getFile().replaceWithText(xml.toString())&&temporary.overwriteTargetFileWithTemporary();
}
enum class ChainStatus{calibrated,partial,disabled,setup};
inline juce::String chainName(ChainStatus s)
{return s==ChainStatus::calibrated?"CHAIN CALIBRATED":s==ChainStatus::partial?"PARTIAL CALIBRATION":s==ChainStatus::disabled?"CALIBRATION DISABLED":"SETUP REQUIRED";}
}
