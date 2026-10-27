# ToneGrid Dattorro reverb prototype

Project: el-visio/dattorro-verb, by El Visio (2022).
Source: https://github.com/el-visio/dattorro-verb
Pinned commit: 41e976a0228a2472156f5c010b3bfbb444e777d2
License: MIT. Copyright (c) 2022 Pauli Pölkki.
Complete required notice: DATTORRO-MIT.txt (distributed alongside this file).

The upstream verb.c, verb.h and verb_structs.h are preserved verbatim under
vendor/dattorro-verb. Only the processing/setter functions are used. ToneGrid's
DattorroReverbCore owns/preallocates the delay buffers, reproduces the upstream
delay/tap table, scales it in prepare to the sample rate, and owns a simplified
musical control mapping. Upstream create/delete/malloc code is not called by
ToneGrid. Predelay interpolation, control smoothing, wet gain, bypass, candidate
crossfade, persistence and UI are ToneGrid additions outside upstream source.

The table reference is 29,761 Hz, confirmed in Table 1 of Jon Dattorro,
Effect Design Part 1: Reverberator and Other Filters, JAES 1997, p. 664:
https://ccrma.stanford.edu/~dattorro/EffectDesignPart1.pdf
The paper is a design reference, not redistributed in the application.

The baseline uses the already bundled JUCE 8.0.6 juce::Reverb (Freeverb-style).
Copyright (c) Raw Material Software Limited. JUCE's existing AGPLv3/commercial
licensing remains applicable; this baseline is not MIT licensed. See JUCE.md.
No JUCE source was modified and no third reverb library was integrated.

Room, Plate and Large are parameter voicings of the same Dattorro topology.
No claim of TONEX equivalence, Spring simulation or final listening validation.
Keep the entire Licenses directory with every executable distribution.
