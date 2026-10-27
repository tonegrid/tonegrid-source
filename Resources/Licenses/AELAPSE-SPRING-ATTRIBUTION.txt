# ÆLAPSE Spring DSP extraction for ToneGrid

Source authors/projects: **smiarx / S-audio**, ÆLAPSE and smiarx/dsp. Original projects:
https://github.com/smiarx/aelapse and https://github.com/smiarx/dsp .

Release pin: ÆLAPSE **v1.0.1**, commit `ef108369d826aa180b8bc8bc319aa7d6ac90061a`.
DSP submodule pin: `f28c084b3269716f46a7d2fc9f64e3bb5bcb6f21`.
GCEM submodule pin: `012ae73c6d0a2cb09ffe86475f5c6fba3926e200`.
Original and adapted SHA-256 hashes, exact source list and GCEM header hashes are in `UPSTREAM.json`.

## License notices

ÆLAPSE is GPL-3.0. Its exact release license is `AELAPSE-GPL-3.0.txt`.
The pinned DSP submodule did not include a LICENSE file. The same upstream repository subsequently added its GPL-3.0 notice on 2026-06-01, commit `763a4136fdd83aa76659d0a910787b80bd245a8b`; that exact notice is preserved as `DSP-GPL-3.0.txt`. This later notice is recorded separately, not represented as a file present in the original February pin.
GCEM (GCE-Math, Copyright 2016-2024 Keith O'Hara) is Apache-2.0, with exact `GCEM-LICENSE.txt` and `GCEM-NOTICE.txt`. Existing GCEM per-file notices remain intact.
The upstream Spring/DSP files have no individual copyright header. ToneGrid does not invent copyright years or a personal name for their author.
All four license/notice files and this attribution are copied to the executable's `Licenses` directory. No source license is replaced by an MIT claim. This GPL-derived integration does not provide a proprietary-distribution exemption; distribution of a combined binary must be accompanied by the corresponding source/license obligations, including modified sources and build inputs. Copying only the license texts is not a source-delivery mechanism. No public distribution is performed by this build.

## Minimal integrated sources

Only `processors/springs/{Springs.h,Springs.cpp,SpringsDefines.h}` and their 23 reachable `include/dsp` headers are imported. GCEM's header-only include tree is retained intact. Optional SPRINGS_RMS/SPRINGS_SHAKE are disabled. No ÆLAPSE plugin wrapper, tape delay, presets, GUI, OpenGL, fonts, CPU dispatch, reader-writer queue, upstream CMake or OpenMP is compiled.

## ToneGrid modifications, 2026-10-01

- `Springs.h/.cpp`: converters are owned per instance and released with prepared buffers; deterministic private chaos/modulation seeds; explicit reset clears histories and finishes existing smoothers without reconstruction; decimation factor limited to 1..8; fully wet endpoint sets dry gain exactly zero.
- `Buffer.h`: reset/rewind of buffer cursors.
- `Smoother.h`: retain target and add finish for reset, preserving normal step equations.
- `Context.h`: dependent template static assertion for compiler portability.
- `cpu/defines.h`: recognize MSVC x64/SSE2 and name scalar fallback.
- `simd/simd_x86_64.h`: explicit SSE bit-cast intrinsics replace GCC-style vector casts; directly invoked inline generic wrappers replace intrinsic function pointers unsupported by MSVC. No numerical conversion or AVX2 compiler flag is added.

ToneGrid's separate `SpringReverbProcessor` owns preparation and frames processing into fixed 32-sample chunks. This adds 32 samples of wet-only framing latency; dry audio is never delayed. It keeps native dispersion, feedback, tone and resampling algorithms. Common controls map outside the source core; documentation is in `Tests/SPRING_REVERB_REPORT.md`.

Platform-neutral C++20 source and upstream SSE/NEON/scalar paths are retained. Only Windows x64/MSVC has been compiled/tested in this milestone; other platforms are not claimed validated.
