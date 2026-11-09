# UNIT808

**UNIT808 Rhythm Composer** by Transistor Labs: an 808-style drum machine plug-in (AU / VST3 / LV2 / Standalone) for
macOS, Windows and Linux. The sound engine is the one in the UNIT 808 Max for Live device, ported to C++
(all sounds synthesized, no samples), behind a hardware-style panel and an 808-style sequencer.

## Using it

| Control | What it does |
|---|---|
| MODE | Pattern Clear, 1st Part, 2nd Part (writing), Manual Play, Play (rhythm track), Compose (write a track) |
| INSTRUMENT-SELECT | The instrument the step keys write (AC BD SD LT MT HT RS CP CB CY OH CH). In Play / Compose: rhythm track 1-12 |
| STEP keys 1-16 | 1st/2nd Part: write steps (drag to paint). Other modes: keys 1-12 choose Basic Rhythm 1-12, 13-16 choose Intro/Fill 1-4 |
| CLEAR | Pattern Clear: clear the rhythm. Compose: clear the track. 1st/2nd Part: CLEAR + step key = step number (or Shift-click); Alt-click clears the instrument |
| TAP | Fill in trigger while playing, tap-write in 1st/2nd Part, add the chosen rhythm to the track in Compose |
| BASIC-VARIATION / I/F-VARIATION | A, AB, B for the rhythm; A or B for the fill |
| MEASURES AUTO FILL IN | Fill every 16, 12, 8, 4 or 2 bars (Manual = off) |
| PRE-SCALE | 1 = 16th triplets, 2 = 32nds, 3 = 16ths, 4 = 8th triplets |
| SWING / FINE SWING | Delay every second step (tempo always follows the DAW) |
| START/STOP | Shows the DAW transport (standalone: starts/stops, right-click for tempo) |
| Sound knobs and switches | Levels, tone, tuning, snappy, decay; tom/conga, rim shot/claves, clap/maracas |
| Transistor Labs logo | About window, contact, store, built-in manual |

MIDI notes (General MIDI drum map) play the sounds. Everything is saved with your project; every control can be
automated and MIDI-mapped.

The manual is `docs/UNIT808 Manual.pdf`, the release notes `docs/UNIT808 Update History.pdf`; both come with every build.

## Building

CMake 3.22+ and a C++17 compiler; JUCE 8.0.9 is downloaded automatically.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

GitHub Actions builds macOS (.pkg), Windows (Setup.exe) and Linux (.tar.gz) on every push (under the run's
**Artifacts**); pushing a tag `vX.Y.Z`, or a commit on main with `[release]` in its message, publishes a GitHub Release with all installers and the PDFs.

## Artwork and docs

`assets/ui_src` is the vector kit (faceplate, every control state, `control_layout.json`). `tools/rasterise_ui.py`
renders it to `assets/ui/*.png` (2x) and writes `Source/Layout.h`. `docs/manual/build_docs.py VERSION` rebuilds the PDFs.
