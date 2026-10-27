#pragma once
#include <NAM/dsp.h>
#include <json.hpp>
#include <juce_core/juce_core.h>
#include <optional>
#include <cmath>

namespace tonegrid::calibration
{
inline bool validLevel(double value) noexcept
{return std::isfinite(value)&&value>=-60.0&&value<=60.0;}
enum class CaptureStatus { calibrated,partial,uncalibrated,invalid };
struct Metadata
{
    std::optional<double> input,output;
    bool invalid=false;
    juce::String gearType; // Presentation/context only; never affects calibration gains.
    CaptureStatus status() const noexcept
    {return invalid?CaptureStatus::invalid:input&&output?CaptureStatus::calibrated:input||output?CaptureStatus::partial:CaptureStatus::uncalibrated;}
    bool complete() const noexcept{return status()==CaptureStatus::calibrated;}
    juce::String description() const
    {
        const auto name=status()==CaptureStatus::calibrated?"Calibrated":status()==CaptureStatus::partial?"Partially Calibrated":status()==CaptureStatus::invalid?"Invalid Calibration Data":"Uncalibrated";
        auto text=juce::String(name)+"\nInput: "+(input?juce::String(*input,2)+" dBu":"Not available")
            +"\nOutput: "+(output?juce::String(*output,2)+" dBu":"Not available");
        if(invalid)text+="\nInvalid or inconsistent calibration data: automatic compensation is not used for this capture.";
        return text+"\nMetadata completeness does not verify the electrical accuracy of the capture.";
    }
};
inline Metadata metadata(const nlohmann::json& json)
{
    Metadata result;
    if(!json.is_object()){result.invalid=true;return result;}
    const auto found=json.find("metadata");if(found==json.end()||found->is_null())return result;
    if(!found->is_object()){result.invalid=true;return result;}
    if(auto gear=found->find("gear_type");gear!=found->end()&&gear->is_string())result.gearType=juce::String(gear->get<std::string>()).trim();
    auto read=[&](const char* key,std::optional<double>& destination)
    {
        const auto value=found->find(key);if(value==found->end())return;
        if(!value->is_number()){result.invalid=true;return;}
        const double number=value->get<double>();
        if(validLevel(number))destination=number;else result.invalid=true;
    };
    read("input_level_dbu",result.input);read("output_level_dbu",result.output);
    return result;
}
inline Metadata metadata(nam::DSP& dsp) noexcept
{
    Metadata result;
    if(dsp.HasInputLevel()){const auto value=dsp.GetInputLevel();if(validLevel(value))result.input=value;else result.invalid=true;}
    if(dsp.HasOutputLevel()){const auto value=dsp.GetOutputLevel();if(validLevel(value))result.output=value;else result.invalid=true;}
    return result;
}
// Local loading copy only: never rewrite a capture or its trained head_scale.
inline void sanitiseMetadata(nlohmann::json& json)
{
    if(json.is_object())if(auto found=json.find("metadata");found!=json.end())
    {
        if(!found->is_object())json.erase(found);
        else for(const auto* tag:{"input_level_dbu","output_level_dbu","loudness"})
        {
            const auto value=found->find(tag);if(value==found->end())continue;
            if(!value->is_number()||!std::isfinite(value->get<double>())
               ||(juce::String(tag)!="loudness"&&!validLevel(value->get<double>())))found->erase(value);
        }
    }
    if(json.is_object())for(auto& item:json.items())
        {if(item.key()!="weights"&&(item.value().is_object()||item.value().is_array()))sanitiseMetadata(item.value());}
    else if(json.is_array())for(auto& value:json)if(value.is_object()||value.is_array())sanitiseMetadata(value);
}
}
