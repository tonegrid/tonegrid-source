# ToneGrid Transparent OD — Pilot Asset Pack v2

Target application version: ToneGrid 1.1.8.

This pack supersedes Pilot v1.

## Corrections
- `face.png`: first knob label is **Input trim**.
- `knob_strip.png`: 101 frames, exactly **one** pointer per frame.
- `originals/`: untouched v1 graphical assets for rollback.
- LED, footswitch and global background assets are unchanged.

## Integration
Use the normal `assets/` tree as the implementation source of truth.
Do not load files from `originals/`.

JUCE must NOT draw an extra rotary pointer, tick, arc, or marker over the knob filmstrip.
LED state swaps `led_off.png` / `led_on.png`.
Footswitch state swaps `footswitch_up.png` / `footswitch_down.png`.

Preset and MIDI remain dynamic JUCE controls using the approved transparent-outline custom LookAndFeel.

Reference logical pedal size: 300 × 450.
Production face asset: 600 × 900.
