#pragma once
#include "LevelCalibration.h"

namespace tonegrid::calibration
{
// UI state only. Capture metadata and chain completeness do not participate.
enum class InterfaceStatus { active, setup, verification, disabled };
inline InterfaceStatus interfaceStatus(const Settings& settings, bool deviceMatches=true) noexcept
{
    if (!settings.enabled) return InterfaceStatus::disabled;
    if (!settings.completed || !valid(settings) || settings.deviceType.isEmpty()
        || settings.deviceName.isEmpty() || settings.channel < 0)
        return InterfaceStatus::setup;
    if (settings.needsVerification || !deviceMatches) return InterfaceStatus::verification;
    return InterfaceStatus::active;
}
inline juce::String interfaceStatusName(InterfaceStatus status)
{
    switch (status)
    {
        case InterfaceStatus::active: return "GUITAR INTERFACE CONFIGURED / ACTIVE";
        case InterfaceStatus::verification: return "GUITAR INTERFACE NEEDS VERIFICATION";
        case InterfaceStatus::disabled: return "GUITAR CALIBRATION DISABLED";
        case InterfaceStatus::setup: return "GUITAR INTERFACE SETUP REQUIRED";
    }
    return {};
}
}
