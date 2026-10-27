#pragma once
#include <juce_core/juce_core.h>
#include <NAM/get_dsp.h>
#include "LevelCalibration.h"
namespace board {
struct Model
{
    juce::String label, file, hash, details, section, creatorUrl;
    std::unique_ptr<nam::DSP> dsp;
    float inputGain = 1, outputGain = 1;
    tonegrid::calibration::Metadata calibration;
};
}
