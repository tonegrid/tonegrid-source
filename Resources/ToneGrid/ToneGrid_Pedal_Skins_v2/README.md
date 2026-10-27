# ToneGrid pedal skins — corrected v2

13 complete pedal enclosure backgrounds, cropped directly from the supplied skin sheet. Each PNG is 224 × 288 pixels, RGBA. All original filenames and pedal IDs are retained for replacement compatibility.

## Correction
The v1 crop coordinates selected partial and neighboring enclosures. This pack uses individually inspected enclosure bounds in the actual 1774 × 887 source image (the image's printed 2048 × 1024 caption is not its real resolution). External labels, neighboring pedals, and side jacks are excluded. The full enclosure face and bevel remain. No artwork has been generated or reconstructed.

Source crops are resampled proportionally to fit 208 × 272 pixels and centered on identical 224 × 288 canvases. Outer padding is transparent. Small areas of original dark background/shadow at the rounded enclosure corners remain inside the rectangular crops; these are not fully alpha-matted cutouts. Source proportions differ slightly, so visible enclosure widths also differ slightly.

## JUCE integration
Use each PNG as a background, preserve its aspect ratio, and draw live controls and labels above it. Do not crop-to-fill. The manifest gives canvas dimensions, source crop coordinates, content rectangles, order, type, expected knobs, and SHA-256 hashes. Coordinates are top-left x, y, width, height, in pixels. The graphics printed on Graphic EQ are artwork, not functional sliders or meters.

No knobs, LEDs, switches, dropdowns, MIDI buttons, or external text are baked into these assets. Pedal names should be rendered by the application.

## Order
1. Classic Fuzz
2. Treble Boost
3. Auto Wah
4. Studio Comp
5. Transparent OD
6. Horse OD
7. 808 OD
8. Blues OD
9. Filter Dist
10. Orange Dist
11. Vintage Fuzz
12. Graphic EQ
13. Clean Boost

Recommended layout: seven pedals on the first row, six on the second. Guitar In and Noise Gate are utility modules on the source sheet and are excluded from this 13-pedal pack.

## Control metadata
NAM pedals: Input trim / Mix / Level.
Algorithmic control metadata is preserved from v1: Auto Wah = Sensitivity / Range / Decay; Studio Comp = Threshold / Ratio / Level; Graphic EQ = Low / Mid / High; Clean Boost = Boost / Mix / Level. This metadata is an integration convention, not a new DSP specification. Implement an actual graphic equalizer's band controls according to the application's DSP.

The separate preview contact sheet is for review only, not a pedal background asset.
