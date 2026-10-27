# stft-pitch in ToneGrid 1.1.9

Upstream: https://github.com/jurihock/stftPitchShift
Pinned commit: `8e55dc639811a8ac5b3f5bc3d1f90a11c59ca582`. Tag: v2.0.
License: MIT; preserve the complete supplied LICENSE in copies or substantial portions.
All copied upstream files are unchanged; source-manifest.json lists paths and SHA-256.
License is also shipped under Resources/Licenses and beside the executable.

Only Vocoder/Pitcher/Resampler/Arctangent are integrated. ToneGrid-owned PitchEngine confines their API to one implementation, uses a ToneGrid-owned radix-2 FFT with precomputed twiddles/permutation and no execution lock, preallocates streaming rings and overlap/add storage, and adapts the asymmetric Hann window formulas from upstream cpp/StftPitchShift/STFT.h (same pinned MIT source). High-level offline STFT/FFT/timer/formant/normalizer APIs are not imported. JUCE's Windows fallback FFT was evaluated then replaced because its perform() takes a SpinLock; no JUCE source was changed. The streaming alignment is performed in the owned wrapper, so Vocoder uses its symmetric-phase constructor to avoid double time shifting. This is a causal streaming adaptation, not a claim of identical offline high-level output.
