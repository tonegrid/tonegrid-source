#pragma once
#include <juce_core/juce_core.h>

namespace tonegrid::cabinet
{
enum class CaptureKind { unknown, fullRig, head, preamp, poweramp };
inline CaptureKind kind(juce::String gear)
{
    gear=gear.toLowerCase().trim();
    if(juce::StringArray{"amp-cab","amp_cab","full-rig","full_rig","amp+cab","amp + cab","amp_with_cab"}.contains(gear))return CaptureKind::fullRig;
    if(juce::StringArray{"amp","amp-head","head","head-only","amp_only","amp only","amp_head"}.contains(gear))return CaptureKind::head;
    if(gear=="preamp")return CaptureKind::preamp;
    if(gear=="poweramp")return CaptureKind::poweramp;
    return CaptureKind::unknown;
}
inline juce::String label(CaptureKind value)
{return value==CaptureKind::fullRig?"HEAD + CAB":value==CaptureKind::head?"HEAD ONLY":value==CaptureKind::preamp?"PREAMP ONLY":value==CaptureKind::poweramp?"POWER AMP ONLY":"TYPE UNKNOWN";}
inline bool needsIR(CaptureKind value){return value==CaptureKind::head||value==CaptureKind::preamp||value==CaptureKind::poweramp;}
}
