#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
namespace board {
struct CompressorPreset { const char* name; float threshold, ratio, attack, release, mix, makeup; };
inline constexpr std::array<CompressorPreset, 8> compressors {{
    { "01 Gentle", -18, 2, 25, 150, 1, 2 }, { "02 Picking", -24, 3, 15, 120, .7f, 3 },
    { "03 Funk", -28, 4, 8, 80, .8f, 4 }, { "04 Country", -30, 6, 3, 100, 1, 5 },
    { "05 Sustain", -32, 5, 10, 240, .9f, 6 }, { "06 Parallel", -32, 8, 5, 160, .4f, 3 },
    { "07 Humbuckers", -14, 3, 20, 120, .75f, 2 }, { "08 Fast control", -20, 8, 1, 70, 1, 2 }
}};
class CompressorProcessor {
public:
    void reset() noexcept { envelope = 0; }
    void process(const float* input,float* output,const float* thresholds,int length,float ratio,
                 double sr,const CompressorPreset& preset) noexcept {
        const float attack = (float)std::exp(-1.0 / (sr * preset.attack * .001));
        const float release = (float)std::exp(-1.0 / (sr * preset.release * .001));
        for (int j=0;j<length;++j) {
            const auto k = (size_t)j; const float level = std::abs(input[k]);
            const float coeff = level > envelope ? attack : release;
            envelope = coeff * envelope + (1 - coeff) * level;
            const float over = juce::jmax(0.f, juce::Decibels::gainToDecibels(envelope, -100.f) - thresholds[k]);
            output[k] = input[k] * juce::Decibels::decibelsToGain(-over * (1 - 1/ratio));
        }
    }
private:
    float envelope = 0;
};
}
