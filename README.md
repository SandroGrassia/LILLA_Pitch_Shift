# Granular

Minimal granular pitch-shift experiment for the existing Lilla PCB 2026 hardware.
One fixed mono source, 2, 4, 8 or 16 overlapping grains, original transport speed.
No MIDI, display, effects, recording, terminal commands, or Flash writes.

## Controls

| Encoder | Rotation | Pushbutton |
| --- | --- | --- |
| Step | Pitch in 10-cent steps, ratio 0.25 to 4 | Start/stop |
| From | Output grain duration: 5, 20, 50, 100 ms | Cycle grain count: 4 -> 8 -> 16 -> 2 -> 4 |
| To | Symmetric linear attack/release, 5 to 50 percent per edge | Unused |
| LineOutVol | Output gain, 0 to 100 percent | Unused |

Startup: stopped, pitch 1, grain 50 ms, slope 25 percent per edge, volume 25 percent, four active grains.
Start after a completed stop begins at sample zero. Start/stop use approximately
5 ms fades. Pitch and volume are smoothed per sample with a 256-sample time constant.
Pitch changes apply to active grains without resetting their read pointers.
Slope changes apply to newly started grains. Duration and grain-count changes use approximately
5 ms fade-out and 5 ms fade-in and restore evenly staggered grains while transport
continues. This deliberately simple transition prevents grains from synchronizing
when changing between durations that are exact multiples.

## Source and hardware

Adv_01.raw must already exist under that exact name in the SerialFlash filesystem.
It is headerless mono signed 16-bit little-endian PCM at 44.1 kHz, as in Lilla.
The complete file is copied to the PSRAM allocation pool at startup. Flash is read
only; no source audio is included in this repository. Missing file, invalid size,
missing PSRAM, or allocation failure disables playback and produces a Serial message.
The Teensyduino 1.62 core recognizes Lilla's two ISSI 16 MiB PSRAM chips.

Audio Board Rev D: standard Teensy I2S and SGTL5000 control connections.
Identical mono output is routed to both I2S channels. Line-out level is 13, matching
Lilla; headphone level is 0.8. LineOutVol adjusts the generated PCM gain.
Flash uses SPI and CS 6. Controls use SPI1 (MOSI 26, SCK 27, MISO 39, CS 37).
Display CS 38 is kept high.

| Control | MCP23S17 chip | DT | CLK | Button |
| --- | --- | --- | --- | --- |
| Step | 3 | a3 | a4 | a2 |
| From | 3 | a6 | a7 | a5 |
| To | 3 | a1 | a0 | unused |
| LineOutVol | 4 | a2 | a1 | unused |

Control scanning runs in loop at up to 2 kHz, with a full-cycle quadrature decoder using Lilla's green-encoder
rotation polarity and a 25 ms Step-button debounce. No controller work runs in
the audio interrupt.

## Audio core

Each grain has its own integer source index and fractional phase, linear
interpolation, age, length, and trapezoidal window. Source indices wrap cyclically.
At steady settings, starts are separated by the grain duration divided by the selected grain count.
Transport advances exactly one source sample per output sample, independent of
pitch. Grain readers advance by the smoothed pitch ratio. Windowed values are
summed and divided by the sum of window weights before volume and gate fades.
Primed grains of age a start reading at T + (pitch - 1) * a, where T is
the current transport. At pitch 1 all grains read the current sample, including
startup and density/duration transitions, apart from gain/fades and integer rounding.

The source is cyclic, including the pre-roll used to prime the active grains.
There is no dedicated treatment of an abrupt file-end discontinuity.
This is a listening/measurement prototype: no anti-alias filter for upward
transposition, transient alignment, formant correction, automatic pitch-dependent density, or
quality guarantee for complex material. Grain count is selected explicitly with the From pushbutton; startup is four.
Increasing density increases CPU and PSRAM traffic; monitor DIAG at 16 grains.

## Build and use

The package versions match the current Lilla PlatformIO project. Dependencies
are installed by PlatformIO; no local modification of the Teensy framework is needed.

```powershell
pio run
pio device monitor --baud 115200
```

The generated firmware is .pio/build/teensy41/firmware.hex. A matching prebuilt
Granular_v0_1_2.hex is supplied at the repository root. Load it with Teensy Loader
when ready to test the hardware. Firmware loading was not performed during project
creation. Restoring the Lilla firmware restores the original application.

Serial output reports initialization, control values and a diagnostic line every
2 seconds: maximum core rendering time, maximum whole-audio CPU usage, audio-pool
peak and missed allocations. Lines are dropped when the terminal has insufficient
buffer space, so printing does not block controller scanning. No Serial input is read.

## Validation

```powershell
g++ -std=c++20 -O2 -Wall -Wextra -Werror -I include src/GranularCore.cpp tests/core_test.cpp -o tests/core_test.exe
./tests/core_test.exe
```

Run `python tests/encoder_test.py` to verify both rotation directions, repeated
polls, contact bounce, and incomplete cycles.

Host tests check source reconstruction at pitch 1 across every duration and
representative slopes, measured tone transposition at ratios 0.25/0.5/1/2/4,
DC normalization, short-source wrapping, pitch changes,
all duration changes including 20-to-5 ms, fixed transport speed, and stop/restart.
The Teensy firmware build is also required. Audio quality, encoder response,
actual Flash contents, PSRAM transfer integrity, and worst-case timing must be
verified on physical Lilla hardware.

First listening pass: leave pitch at 1 and compare the four grain durations; then
try 0.5, 2, 0.25 and 4 while varying duration and slope. Record the diagnostics and
listen especially for attacks, periodic coloration, and file-wrap discontinuities.

## Provenance

Hardware mapping and the basis of the corrected green-encoder transition table derive from Sandro Grassia's
Lilla firmware (source commit 283739101236e5ebba9e0c3b307b56c8177b23e9).
The granular core is implemented specifically for this experiment.
Teensy Audio, SerialFlash, and Adafruit dependencies retain their own licenses.

Version 0.1.2 fixes initial grain read positions. Regression tests compare the
first samples with an independent grain birth-time model at six pitch ratios,
and check unity reconstruction through duration/density fades.
