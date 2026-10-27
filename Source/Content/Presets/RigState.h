#pragma once
#include "Effects/Pedalboard/PedalBoard.h"
#include "AmpSource.h"
#include "CabinetState.h"
#include "NamAmpState.h"
#include "OutputDspState.h"
#include <functional>

namespace board
{
// Value snapshot only. Capturing/applying the live engine is the host's job.
// Setting/InputSettings stay in their existing headers: no parameter/DSP refactor.
struct State { std::optional<tonegrid::cabinet::State> cabinet; String name, presetId, extensionXml; MemoryBlock amp; std::array<Setting,count> pedals; InputSettings input; bool genericVst3=false; amp::SourceType sourceType=amp::SourceType::Vst3; namamp::State namAmp; std::optional<tonegrid::output::Settings> outputDsp; StringArray missingCaptureDiagnostics; std::array<String,count> customPedalHashes; };
using RigIdentityLookup = std::function<String(int persistentId, int preset)>;
}
