#pragma once
#include <cmath>
#include "Effects/Reverb/ReverbParameters.h"
#include "EffectsParameters.h"
#include "Effects/AmpEq/AmpEqParameters.h"
namespace tonegrid::output
{
// Plain values shared by control/state adapters. No UI, NAM, VST or OS types.
struct Settings
{
    bool ampNormalizationEnabled=false;
    bool ampNormalizationEnhanced=true; // Legacy ON includes the optional ToneGrid correction.
    bool spreadEnabled=false;
    float offsetMs=15,wobblePercent=25,crossoverHz=130;
    bool wobbleEnabled=true,crossoverEnabled=true,diffuseEnabled=true;
    reverb::Parameters reverb;
    pitch::Parameters pitch;
    mod::Parameters mod;
    delay::Parameters delay;
    ampEq::Parameters ampEq;
    bool expandedAmpEq=false;
    bool expandedEffects=false; // Explicit v4/rig12, untouched old defaults stay byte-compatible.
};
inline float selectedNormalizationTrim(const Settings& s,float measuredTrim) noexcept
{return s.ampNormalizationEnabled&&s.ampNormalizationEnhanced?measuredTrim:0.f;}
inline Settings freshSessionSettings() noexcept
{Settings s;s.ampNormalizationEnabled=true;s.ampEq.enabled=false;return s;}
inline bool requiresAmpEq(const Settings& s) noexcept{return s.expandedAmpEq||ampEq::advanced(s.ampEq)||!ampEq::flat(s.ampEq);}
inline bool requiresExpanded(const Settings& s) noexcept
{
    const pitch::Parameters p;const mod::Parameters m;const delay::Parameters d;
    return s.expandedEffects||s.pitch.mode!=p.mode||s.pitch.semitones!=p.semitones||s.pitch.dry!=p.dry||s.pitch.down!=p.down||s.pitch.up!=p.up
        ||s.mod.enabled!=m.enabled||s.mod.type!=m.type||s.mod.rateHz!=m.rateHz||s.mod.depth!=m.depth||s.mod.level!=m.level
        ||s.delay.enabled!=d.enabled||s.delay.timeMs!=d.timeMs||s.delay.feedback!=d.feedback||s.delay.mix!=d.mix;
}
inline bool valid(const Settings& s) noexcept
{
    return std::isfinite(s.offsetMs)&&s.offsetMs>=-24&&s.offsetMs<=24
        &&std::isfinite(s.wobblePercent)&&s.wobblePercent>=0&&s.wobblePercent<=100
        &&std::isfinite(s.crossoverHz)&&s.crossoverHz>=32.5f&&s.crossoverHz<=520&&reverb::valid(s.reverb)
        &&ampEq::valid(s.ampEq)&&pitch::valid(s.pitch)&&mod::valid(s.mod)&&delay::valid(s.delay);
}
inline Settings legacySettings() noexcept {Settings s;s.ampNormalizationEnabled=false;return s;}
}
