#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace tonegrid::calibration
{
struct RawReading { float peak=0; bool connected=false; };
enum class InputStatus { waiting, detected, clipping, unavailable };
// UI-side interpretation of the existing ADC peak tap. This identifies input
// activity, not a guitar, electrical calibration, or a target playing level.
struct Verification
{
    double started=-1, previousTime=-1, candidate=-1, detectedUntil=-1, unsafeUntil=0;
    float floorDb=-100;
    bool connected=false;
    void update(RawReading reading, double now) noexcept
    {
        connected=reading.connected;
        if (!connected) { started=-1; candidate=detectedUntil=-1; return; }
        const float db=juce::Decibels::gainToDecibels(std::abs(reading.peak),-100.f);
        if (started<0) { started=now; previousTime=now; floorDb=db; }
        const double dt=juce::jlimit(0.0,.25,now-previousTime); previousTime=now;
        if (!std::isfinite(reading.peak) || std::abs(reading.peak)>=.89125094f)
        { unsafeUntil=now+2; candidate=detectedUntil=-1; return; }
        // Learn a short initial background, then require a sustained rise over
        // that floor. Steady ordinary noise and isolated clicks are insufficient.
        if (now-started<.25) { floorDb=juce::jmin(floorDb,db); return; }
        const bool activity=db>=-70.f && db>=floorDb+8.f;
        if (activity)
        {
            if (candidate<0) candidate=now;
            if (now-candidate>=.08 && now>=unsafeUntil) detectedUntil=now+8;
        }
        else { candidate=-1; floorDb=juce::jmin(db,floorDb+float(dt)); }
    }
    InputStatus status(double now) const noexcept
    {
        if (!connected) return InputStatus::unavailable;
        if (now<unsafeUntil) return InputStatus::clipping;
        return now<=detectedUntil ? InputStatus::detected : InputStatus::waiting;
    }
    bool safe(bool available,double now) const noexcept
    { return available && status(now)==InputStatus::detected; }
    bool manualAvailable(double now) const noexcept
    { return status(now)==InputStatus::waiting && started>=0 && now-started>=1.5; }
};
}
