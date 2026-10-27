# ToneGrid integration — Transparent OD pilot

The ten files unpacked from the user-supplied pilot archive remain byte-identical. Runtime assets are embedded under namespace `TransparentOdAssets`. The background/preview are references only and are not used to change the global UI.

`PedalAssetSkin.h` provides UI-only metadata/layout/image rendering. `PedalCard` opts in only for historical TransparentOd ID 4, and retains its original state/parameter/preset/MIDI callbacks. Other cards use their unchanged renderer.

The original JSON calls the middle rotary `mix`, but the face and preview both print TONE. Current ToneGrid uses post-NAM Tone (-6 to +6 dB), not parallel Mix. The pilot retains this binding and the original JSON bytes. The first baked GAIN label corresponds to the existing Input Trim (-12 to +12 dB). Existing tooltips describe the real parameters.

No title, subtitle, knob labels, metal, LED glow, texture, side sockets or extra shadows are painted procedurally for the pilot. JUCE paints only the custom transparent preset/MIDI outlines/text/chevron and manages interaction. The empty title hover component retains existing real NAM metadata.

Logical bounds are derived from the metadata's reference size and fitted proportionally. This does not impose a universal 300px width. A future 340x450 asset can share the mapping; Graphic EQ is not migrated here.

Validation and build instructions: `Tests/TRANSPARENT_OD_PILOT_REPORT.md` and `Tests/README.md`.
