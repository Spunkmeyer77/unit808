# Lemus circuit voice engine

Upstream source by Lemus, commit
`900ce8d7fc64aa569b2ec698928a93d048afea10`.
https://github.com/lemus-dev/lemus/tree/900ce8d7fc64aa569b2ec698928a93d048afea10

This extract includes a standalone CMake build and this guide. See the
modification notice below and LICENSE for GNU AGPLv3 terms.

| Path under machines/tr808 | Contents |
| --- | --- |
| core/voiceboard.h and .cpp | VoiceBoard API, controls, calibration, triggers and mixing |
| core/circuit.h and .cpp | Netlist parser and circuit solver |
| core/netlists/ | Circuit definitions and component models |
| core/oversampler.* and parallel.* | Audio decimation and voice worker threads |
| docs/model.md and docs/circuit.md | Upstream modelling notes |
| tools/voicerun/ | Upstream hit-to-WAV renderer |
| tools/circuitrun/ | Upstream individual circuit runner |

Some links in the upstream notes refer to other files in the full repository.

## Build

Requires CMake 3.22+ and a C++17 compiler. On macOS, use Apple's Command Line Tools
and CMake. The build produces a static library and two command-line tools.
It has no JUCE or ROM dependency.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/voicerun demo.wav --seconds 1.4 --hit 0.05:bd --hit 0.30:sd --hit 0.55:cp --hit 0.8:ch --hit 1.05:cb
```

Multi-config generators place the executables under `build/Release`.
For an existing CMake project, add this directory with `add_subdirectory`,
link `tr808_voice_engine`, and include `tr808/core/voiceboard.h`. The upstream
`voicerun` source shows preparation, control setup, triggers and rendering.

## Ticco 808 calibration modifications

Transistor Labs, 2026-11-15. The kick netlist exposes resonator capacitors C41
and C42 for tuning. The Max bridge adds a calibration panel, controlled circuit
rebuilds and factory recall. See LICENSE for applicable terms.
