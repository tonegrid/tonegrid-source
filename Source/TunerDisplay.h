#pragma once
#include "TunerEngine.h"

namespace tonegrid::tuning
{
// Message-thread presentation only. The detector and audio path stay untouched.
class Display final
{
public:
    void reset() noexcept { *this = {}; }
    void advance(const Reading& reading, double elapsedSeconds) noexcept
    {
        const double dt = std::clamp(elapsedSeconds, 0.0, 0.1);
        if (!std::isfinite(reading.frequency) || reading.frequency <= 0
            || !std::isfinite(reading.confidence) || reading.confidence < 0.7)
        { reset(); return; }
        const double pitch = 69.0 + 12.0 * std::log2(reading.frequency / 440.0);
        // Bound before converting to int, including malformed/test input.
        if (!std::isfinite(pitch) || pitch < 0 || pitch > 127) { reset(); return; }
        const int nearest = int(std::round(pitch));
        if (!valid)
        {
            valid = true; note = nearest;
            cents = textCents = indicatorCents = 100.0 * (pitch - note);
        }
        // Eight cents of boundary hysteresis and 80 ms of sustained evidence
        // reject fleeting adjacent notes and octave estimates.
        if (nearest != note && std::abs(pitch - note) >= 0.58)
        {
            if (candidate != nearest) { candidate = nearest; candidateSeconds = 0; }
            candidateSeconds += dt;
            if (candidateSeconds >= 0.08)
            {
                note = nearest; cents = textCents = indicatorCents = 100.0 * (pitch - note);
                candidate = -1; candidateSeconds = textSeconds = 0;
            }
        }
        else { candidate = -1; candidateSeconds = 0; }
        // Avoid sweeping across the scale while confirming another string.
        if (nearest != note && std::abs(pitch - note) >= 0.58) return;
        targetCents = 100.0 * (pitch - note);
        cents += (targetCents - cents) * (1.0 - std::exp(-dt / 0.12));
        // Glide into the centre within the green zone rather than snapping there.
        indicatorCents += ((inTune() ? 0.0 : cents) - indicatorCents) * (1.0 - std::exp(-dt / 0.08));
        textSeconds += dt;
        if (textSeconds >= 0.1)
        {
            textCents = std::round(cents * 10.0) / 10.0;
            textSeconds = std::fmod(textSeconds, 0.1);
        }
    }
    bool inTune() const noexcept
    { return valid && candidate < 0 && std::abs(cents) <= 5.0 && std::abs(targetCents) <= 5.0; }
    bool valid = false;
    int note = 0;
    double cents = 0, textCents = 0, indicatorCents = 0;
private:
    int candidate = -1;
    double candidateSeconds = 0, textSeconds = 0, targetCents = 0;
};
}
