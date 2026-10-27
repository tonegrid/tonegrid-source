# ToneGrid 1.1.8 — clean-face fidelity integration

The face is the separately supplied `5837468d-0b43-4010-8d6c-2952d1c7a04e.png`, copied without changing any bytes. SHA-256: `243278990d168361052a7d31ceb382c4d09d2c8e08c4bad9d906e51411b9a04f`. Size 1024 × 1536, RGBA. No PNG retouch, resampling or background removal was performed on disk.

Runtime composition: clean supplied static face + original v3 `knob_strip.png` + clean v1 LED OFF/ON and footswitch UP/DOWN. V3 face is rejected. V3 LED/switch files are preserved in this folder for provenance but are not embedded; its switch contains a tiny baked legend. V1/v2 folders and previous executables remain intact.

The UI-only JSON is the source of overlay positions in a 300 × 450 logical coordinate system. The viewport (20,25,260,390) removes only outside image margins during native JUCE drawing, with uniform scaling. Full original pixels remain in the embedded PNG. Enclosure, labels, motif and branding are preserved. Knob centers (78,112), (150,112), (225,112), diameter 60; LED (150,45), diameter 16; preset (75,220,150,34); MIDI (110,264,80,28); footswitch (150,329), diameter 58. Assigned MIDI text may use the preset width. Title hover remains a non-painted metadata target.

Only the filmstrip supplies the knob visual; no JUCE arc, pointer or second indicator. Read-only inspection of all 101 frames found one dark pointer component in each central cap. Knob painting ignores JUCE's inset rotary rectangle and uses full metadata component bounds. Parameter mappings and real engine/state ownership remain unchanged: Input Trim -12..+12 dB, post-NAM Tone -6..+6 dB, Level -24..+24 dB.

Preset/MIDI outlines and dynamic text are drawn once by the custom LookAndFeel. Font sizes 14/13 logical pixels, minimum 10. There are no baked preset values, MIDI text or moving hardware on the face. No global background change and no conversion of other pedal cards.

See `Tests/TRANSPARENT_OD_FIDELITY_REPORT.md` for exact validation and remaining environment limits.
