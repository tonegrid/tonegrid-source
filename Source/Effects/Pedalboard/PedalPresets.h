#pragma once
#include "Effects/Pedalboard/PedalDefinitions.h"

namespace board
{
// Generated from the installed 1.1.20 catalog by Audits/Pedals119/install-library.py.
// Algorithmic banks retain their eight existing presets. Audio gain is independent.
inline constexpr const char* libraryRevision="tonegrid-library-1.1.38-r2";
inline constexpr std::array<int,count> presetCounts {8,8,18,6,18,16,8,20,24,8,21,20,8};
inline constexpr std::array<int,count> recommendedPresets {0,2,4,2,3,6,0,9,10,0,10,12,0};
constexpr int presetCount(int pedal) noexcept { return pedal>=0&&pedal<count?presetCounts[size_t(pedal)]:0; }
constexpr int recommendedPreset(int pedal) noexcept { return pedal>=0&&pedal<count?recommendedPresets[size_t(pedal)]:0; }
}
