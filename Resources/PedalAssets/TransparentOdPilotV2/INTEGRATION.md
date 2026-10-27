# ToneGrid 1.1.8 — fidelity integration (v2 preview)

All 18 files from the supplied v2 ZIP are preserved byte-for-byte. The v1 folder and earlier executable remain intact. No PNG has been retouched, as requested by the user.

Runtime images are embedded from the v2 `assets/` tree only. Background, previews and originals are reference material. `transparent_od_layout.json` is an additional integration-only metadata file; the supplied `transparent_od.json` remains untouched.

Logical coordinates are 300 × 450. Integration knob centers follow the visible face openings: (62.5, 95), (152.5, 95), (244.5, 95), diameter 58. LED center is (153.5, 35), diameter 16 (14.29% above the v1/v2 metadata diameter). Both ON/OFF use the same bounds. Footswitch remains at supplied (150, 390), diameter 56. Preset/MIDI heights are 34/32 instead of 30/28, with the same compact widths. Assigned MIDI text may use the preset width; unassigned remains compact.

Knob bounds are the metadata bitmap bounds, not JUCE's inset rotary painting area. Only the filmstrip is drawn; no procedural marker is added. Preset/MIDI fonts increase from 12/11 to 14/13 logical pixels, with a minimum of 10 at small sizes. All original parameter mappings are retained: Input Trim -12..+12 dB, post-NAM Tone -6..+6 dB, Level -24..+24 dB. Integration metadata corrects the central identifier to `tone`; DSP/state formats are unchanged.

## Visual approval remains pending

V2 still has a fixed horizontal mark in addition to the moving filmstrip pointer; the face has residual text around Input trim and a footswitch opening above the metadata position. Do not call these fixed by code.

The subsequently supplied v3 was inspected under the ignored build directory only, not integrated. Its sample filmstrip frames show a single pointer. Its face still contains partial moving hardware, baked preset text/chevron/MIDI text, and clipped upper/lower artwork, despite its README. A corrected complete static face is required. Do not repair PNGs in Python.

A replacement face must match its JSON positions, contain only enclosure/texture/static labels/title/subtitle/motif/branding, and exclude all moving hardware and dynamic preset/MIDI content. If outlines are baked, the renderer must respect the JSON render strategy to avoid drawing them twice; current v2 does not bake outlines and uses its custom LookAndFeel.

See `Tests/TRANSPARENT_OD_FIDELITY_REPORT.md` for verification, known issues and exact build paths.
