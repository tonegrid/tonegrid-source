# ToneGrid Transparent OD — Pilot Asset Pack v1

Purpose: production pilot for the ToneGrid 1.1.8 pedal redesign.

## Rendering strategy
Static premium art is supplied as PNG assets. JUCE handles interaction/state only.

### face.png
600×900 (2× asset for a 300×450 reference pedal).
Contains the enclosure, texture, graphic motif, title/subtitle and ToneGrid branding.
No LED, knobs or footswitch are baked into the face.

### knob_strip.png
101 frames, 128×128 each, vertical filmstrip.
Use frame index from normalized control value. Display target ≈58 px at reference scale.

### LED
- led_off.png
- led_on.png
JUCE swaps the image based on pedal bypass state.

### Footswitch
- footswitch_up.png
- footswitch_down.png
JUCE swaps the image while pressed / released.

### Preset + MIDI
Remain JUCE controls using a custom LookAndFeel:
- transparent fill
- thin off-white outline
- off-white text
- no cyan glow
- compact sizing
- preset dropdown 150×30 at reference scale
- MIDI button 80×28 at reference scale

## Important
Do not use JUCE default styling for visible pedal controls.
Do not add side jacks/cables.
Do not bake scene-level shadows into face.png.
The global background is supplied separately.

## Reference layout
300×450 logical coordinate system. See transparent_od.json.

## Background
tonegrid_pedalboard_bg.png is a 2560×1440 subtle anthracite studio surface with central illumination and vignette.

## Pilot workflow
1. Implement Transparent OD only.
2. Validate in the actual ToneGrid UI at normal and HiDPI scaling.
3. Once approved, generalize the component to the other standard pedals.
4. Implement Graphic EQ as the 340×450 wide variant.
