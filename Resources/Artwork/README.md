# Pedal artwork

Source: Chris's ChatGPT Images interface reference, supplied on 28 September 2026.
Adapted with the built-in image_gen tool, using the original as an edit reference.
Final project asset: `pedal-atlas.png`, embedded with JUCE BinaryData.
No text or controls are baked into the texture. Interactive controls are drawn in PedalCard.h.

Generation prompt:

Create a production UI texture atlas derived faithfully from these seven guitar pedal designs. Output landscape 1536x1024. Seven equal-width vertical rectangular pedal faceplates side by side, occupying full canvas height, each exactly one seventh canvas width. NO gaps, NO perspective, NO text, NO letters, NO controls, NO knobs, NO switches, NO screens, NO buttons, NO UI. Pure colored textured enclosure backgrounds only. Preserve embossed black illustrations in UPPER THIRD of each panel with lower TWO THIRDS plain colored textured metal for overlaying real software controls. Left to right: red dragon, green screaming mask, gold rearing mythical horse, orange flaming skull, purple lightning bolts, burgundy ram skull, teal crescent moon and stars. Match the attached design textures and artwork very closely. Each panel fills its complete seventh of the image; equal widths and identical height; no rounded corners or edge padding. Flat straight-on UI background assets. Entire top third illustrations, bottom two thirds richly colored textured metal.

The output's actual panel boundaries are accounted for in PedalCard.h when drawing each card.
