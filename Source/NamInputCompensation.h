#pragma once
#include <json.hpp>
#include <optional>
#include <cmath>

namespace namamp
{
// Publisher listening adjustment before inference, independent of electrical
// calibration and output normalization. Missing/malformed optional data is neutral.
inline std::optional<float> inputCompensation(const nlohmann::json& json)
{
    const auto metadata=json.find("metadata");
    if(metadata==json.end()||!metadata->is_object())return {};
    const auto field=metadata->find("Tonegrid_Input_Compensation");
    if(field==metadata->end()||!field->is_object())return {};
    const auto version=field->find("version"),trim=field->find("trim_db");
    if(version==field->end()||!version->is_number_integer()||*version!=1
       ||trim==field->end()||!trim->is_number())return {};
    const auto db=trim->get<double>();
    if(!std::isfinite(db)||db< -24.||db>24.)return {};
    return static_cast<float>(db);
}
inline float validatedInputCompensation(float db) noexcept
{return std::isfinite(db)&&db>=-24.f&&db<=24.f?db:0.f;}
}
