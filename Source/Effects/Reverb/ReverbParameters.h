#pragma once
#include <cmath>
namespace tonegrid::reverb
{
enum class Type { Room=0, Plate=1, Large=2, Spring=3 };
inline constexpr float minimumTime=.2f,maximumTime=10.f,maximumPreDelay=100.f,maximumPercent=100.f;
struct Parameters
{
    bool enabled=false;
    Type type=Type::Room;
    float timeSeconds=1.5f,preDelayMs=12.f,color=40.f,mixPercent=15.f;
};
inline bool valid(const Parameters& p) noexcept
{
    return int(p.type)>=0&&int(p.type)<=3
        &&std::isfinite(p.timeSeconds)&&p.timeSeconds>=minimumTime&&p.timeSeconds<=maximumTime
        &&std::isfinite(p.preDelayMs)&&p.preDelayMs>=0&&p.preDelayMs<=maximumPreDelay
        &&std::isfinite(p.color)&&p.color>=0&&p.color<=maximumPercent
        &&std::isfinite(p.mixPercent)&&p.mixPercent>=0&&p.mixPercent<=maximumPercent;
}
}
