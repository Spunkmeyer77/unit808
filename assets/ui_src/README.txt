TICCO ROSS UNIT808 — VECTOR UI KIT

CANVAS
1870 x 1090 SVG units. viewBox="0 0 1870 1090".
TR808_preview.png: 1870 x 1090 pixels.
TR808_preview@2x.png: 3740 x 2180 pixels.
The square panel fills the canvas edge to edge without an outer case or rails.
The DAW saves plugin state and supplies the clock and transport.
This package contains visual assets and metadata, without plugin implementation code.

CHANGES IN THIS REVISION
- Removed the rounded outer case, both dark side rails and the old case's two horizontal lines.
- Made the panel background square-cornered and borderless.
- Cropped to the panel and shifted every element, asset viewBox and layout coordinate
  by -65 in x and -30 in y. All screws are preserved.
- Renamed the two clock-related controls to swing and swing-fine, including assets and IDs.
- Printed labels now read SWING, FINE SWING, LESS and MORE. The large 0–10 scale remains.
- Swing defaults to its printed 0 position (-130 degrees); fine swing defaults to 0 degrees/up.
- START/STOP has mode="daw_transport_indicator". Its down state is depressed and darker,
  indicating that the DAW is running. It must not initiate playback.
- Preserved semantic IDs, split knob layers, switch/lever/button states, separate LEDs,
  cream-label padding fixes, the About hotspot and every other knob default angle.
- Regenerated both preview sizes and the text-as-path composite.

PLACEMENT
Assets have global viewBoxes. Use their viewBox origins for exact placement.
Control bounds describe interactive bodies or LED lenses, excluding optional glow/shadow.
Each knob base/cap pair has an identical viewBox centered on its layout cx/cy.
Cap assets point straight up. Rotate them around cx/cy by default_angle for the default state.
Angles are clockwise-positive. Continuous knob limits are -135 to +135 degrees.
Discrete detents follow the printed destinations and override that continuous range.
The reusable LED templates are anchored on the first step LED; individual state files
provide the exact global position for each lens. The invisible About hotspot has no asset.

FILES
TR808_editable.svg — composite with editable lettering and semantic control groups.
TR808_faceplate.svg — static panel, printing and screws only, without moving controls or LEDs.
TR808_outlined.svg — composite with all lettering converted to vector paths.
control_layout.json — control positions, state assets, detents, defaults and DAW transport mode.
label_checks.json — measured cream-label widths and padding.
TR808_preview.png — standard preview.
TR808_preview@2x.png — double-resolution preview.

FULL ASSET INVENTORY
assets/ac-level-base.svg
assets/ac-level-cap.svg
assets/auto-fill-base.svg
assets/auto-fill-cap.svg
assets/basic-variation-pos0.svg
assets/basic-variation-pos1.svg
assets/basic-variation-pos2.svg
assets/bd-decay-base.svg
assets/bd-decay-cap.svg
assets/bd-level-base.svg
assets/bd-level-cap.svg
assets/bd-tone-base.svg
assets/bd-tone-cap.svg
assets/cb-level-base.svg
assets/cb-level-cap.svg
assets/ch-level-base.svg
assets/ch-level-cap.svg
assets/cp-level-base.svg
assets/cp-level-cap.svg
assets/cp-select-pos0.svg
assets/cp-select-pos1.svg
assets/cy-decay-base.svg
assets/cy-decay-cap.svg
assets/cy-level-base.svg
assets/cy-level-cap.svg
assets/cy-tone-base.svg
assets/cy-tone-cap.svg
assets/ht-level-base.svg
assets/ht-level-cap.svg
assets/ht-select-pos0.svg
assets/ht-select-pos1.svg
assets/ht-tuning-base.svg
assets/ht-tuning-cap.svg
assets/if-variation-pos0.svg
assets/if-variation-pos1.svg
assets/instrument-select-base.svg
assets/instrument-select-cap.svg
assets/led-off.svg
assets/led-on.svg
assets/lt-level-base.svg
assets/lt-level-cap.svg
assets/lt-select-pos0.svg
assets/lt-select-pos1.svg
assets/lt-tuning-base.svg
assets/lt-tuning-cap.svg
assets/master-volume-base.svg
assets/master-volume-cap.svg
assets/mode-base.svg
assets/mode-cap.svg
assets/mt-level-base.svg
assets/mt-level-cap.svg
assets/mt-select-pos0.svg
assets/mt-select-pos1.svg
assets/mt-tuning-base.svg
assets/mt-tuning-cap.svg
assets/oh-decay-base.svg
assets/oh-decay-cap.svg
assets/oh-level-base.svg
assets/oh-level-cap.svg
assets/part1-led-off.svg
assets/part1-led-on.svg
assets/part2-led-off.svg
assets/part2-led-on.svg
assets/power-pos0.svg
assets/power-pos1.svg
assets/pre-scale-pos0.svg
assets/pre-scale-pos1.svg
assets/pre-scale-pos2.svg
assets/pre-scale-pos3.svg
assets/rs-level-base.svg
assets/rs-level-cap.svg
assets/rs-select-pos0.svg
assets/rs-select-pos1.svg
assets/sd-level-base.svg
assets/sd-level-cap.svg
assets/sd-snappy-base.svg
assets/sd-snappy-cap.svg
assets/sd-tone-base.svg
assets/sd-tone-cap.svg
assets/start-stop-down.svg
assets/start-stop-up.svg
assets/step-01-down.svg
assets/step-01-led-off.svg
assets/step-01-led-on.svg
assets/step-01-up.svg
assets/step-02-down.svg
assets/step-02-led-off.svg
assets/step-02-led-on.svg
assets/step-02-up.svg
assets/step-03-down.svg
assets/step-03-led-off.svg
assets/step-03-led-on.svg
assets/step-03-up.svg
assets/step-04-down.svg
assets/step-04-led-off.svg
assets/step-04-led-on.svg
assets/step-04-up.svg
assets/step-05-down.svg
assets/step-05-led-off.svg
assets/step-05-led-on.svg
assets/step-05-up.svg
assets/step-06-down.svg
assets/step-06-led-off.svg
assets/step-06-led-on.svg
assets/step-06-up.svg
assets/step-07-down.svg
assets/step-07-led-off.svg
assets/step-07-led-on.svg
assets/step-07-up.svg
assets/step-08-down.svg
assets/step-08-led-off.svg
assets/step-08-led-on.svg
assets/step-08-up.svg
assets/step-09-down.svg
assets/step-09-led-off.svg
assets/step-09-led-on.svg
assets/step-09-up.svg
assets/step-10-down.svg
assets/step-10-led-off.svg
assets/step-10-led-on.svg
assets/step-10-up.svg
assets/step-11-down.svg
assets/step-11-led-off.svg
assets/step-11-led-on.svg
assets/step-11-up.svg
assets/step-12-down.svg
assets/step-12-led-off.svg
assets/step-12-led-on.svg
assets/step-12-up.svg
assets/step-13-down.svg
assets/step-13-led-off.svg
assets/step-13-led-on.svg
assets/step-13-up.svg
assets/step-14-down.svg
assets/step-14-led-off.svg
assets/step-14-led-on.svg
assets/step-14-up.svg
assets/step-15-down.svg
assets/step-15-led-off.svg
assets/step-15-led-on.svg
assets/step-15-up.svg
assets/step-16-down.svg
assets/step-16-led-off.svg
assets/step-16-led-on.svg
assets/step-16-up.svg
assets/swing-base.svg
assets/swing-cap.svg
assets/swing-fine-base.svg
assets/swing-fine-cap.svg
assets/tap-down.svg
assets/tap-up.svg
assets/track-clear-led-off.svg
assets/track-clear-led-on.svg
assets/var-a-led-off.svg
assets/var-a-led-on.svg
assets/var-b-led-off.svg
assets/var-b-led-on.svg
