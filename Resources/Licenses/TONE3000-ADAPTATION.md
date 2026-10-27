# TONE3000 DSP attribution

Source: https://github.com/tone-3000/tone3000-plugin

Pinned commit: `38330d53676daa20c630d0ab1474d4a51ba0ea9b`.

Copyright (c) 2026 TONE3000.

The complete, unmodified MIT license is included in `TONE3000-MIT.txt` alongside this notice. The license and copyright notices must accompany distributions containing these adapted components. CMake copies this directory into the application's `Licenses` directory; keep that directory with the executable.

Adapted material:

- `plugin/src/Processor.cpp`: the final NAM static loudness compensation formula, metadata validity checks and +/-12 dB limits. ToneGrid implements a separate `AmpNormalizationProcessor`, with a 25 ms linear gain ramp, fixed -18 dB target and unity fallback. No calibrated pedal handoff, dynamic normalization or surrounding plugin architecture was imported.
- `plugin/include/Spread.h`, `plugin/src/Spread.cpp`: `StereoSpreadProcessor` parameter mapping, LR4 crossover, four-point Lagrange fractional delay, signed offset smoothing, center blend, precedence gain and bypass crossfades.
- `plugin/include/ImageDeck.h`: the six-stage allpass diffuser, two-pole slow random-walk wobble, crossover mapping and internal correlation meter, isolated in `StereoSpreadDeck.h`.
- `plugin/docs/stereo-image.md`: design reference only; no UI, Align processor or hosting architecture was imported.
- `test/src/spread_tests.cpp`: adapted test concepts including exact bypass/center, signed channel mirroring, crossover, delay, time-invariant wobble bypass, diffusion magnitude, transition continuity and spectral noise regression.

ToneGrid changes: names/namespaces/includes, default offset +15 ms, finite/range validation, invalid sample-rate fallback, mono-output and empty-block handling, independent graph/UI/state adapters and tests. Wobble depth is decoded to zero when its switch is off. DSP primitives retain the upstream algorithms and timings. Active Spread uses channel L as the mono seed; its bypass endpoints preserve each original channel separately.

No new platform-specific DSP dependency. JUCE audio_basics/dsp are required, as already used by the application. Existing ToneGrid dependency licenses remain independently applicable.
