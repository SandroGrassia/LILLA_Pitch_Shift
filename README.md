# LIlla_PitchShift_v_0_0_13

First release of Lilla Pitch Shift for **HW R2**, based on a dual-band FFT engine
that changes pitch while preserving the source playback tempo.

**[Download the firmware](https://github.com/SandroGrassia/LILLA_Pitch_Shift/releases/tag/LIlla_PitchShift_v_0_0_13)**
or use [LIlla_PitchShift_v_0_0_13.hex](LIlla_PitchShift_v_0_0_13.hex) from this repository.

## Features

- Two physical playback voices, shared by Performance, MIDI Loop, preview and
  Live Sampler, including release tails.
- Dual-band FFT processing, with a separate low-frequency analysis branch.
- Forward, reverse and loop playback, including both ping-pong directions.
- Region editing, cross-mix controls, pitch bend and per-note envelopes.
- Live Sampler recording and playback from circular PSRAM buffers.
- Prepared attack analyses for eligible stored samples in PSRAM.
- ADSR, velocity, volume, pan, TONE, VCF/LFO, resolution/downsampling and delay.
- Waveform display with aligned sample groups and partial screen updates.

## Hardware

- Teensy 4.1 at 600 MHz, using the Arduino framework and Teensy Audio library.
- Lilla HW R2 with 256 KB FRAM and 32 MB PSRAM.
- Audio processing in blocks of 128 samples at approximately 44.1 kHz.

## Playback and Live Sampler

A patch can contain up to eight Instrument slots. If a note matches more than
one Instrument, only the first is played. Two slots are the maximum concurrent
voice allocation; release tails also occupy a voice.

Base note transposition spans -24 to +12 semitones relative to the Instrument
root key. Pitch bend adds +/-2 semitones. At unity pitch, samples come directly
from the source while FFT histories remain active for subsequent pitch changes.

The modes are ONCE_FWD, ONCE_REV, LOOP_FWD, LOOP_REV, LOOP_FWD_REV and
LOOP_REV_FWD. Ping-pong modes do not use the end/start cross-mix.

Stored samples are intended to play from PSRAM. Two temporary attack-cache
entries reduce preparation work for eligible stored samples. Preparation takes
place while all voices are idle; Live Sampler does not use this cache.

Live Sampler services I2S input before FFT processing to maintain continuous
capture with overlapping notes. Its preset release is **1 second**, with a
**2-second decay**. Unrecorded samples are guarded during initial buffer fill.

Pitch processing requires source history and preparation, so Live Sampler has
latency. Forward playback while recording requires about 116 ms of source
margin; preparation and I/O buffering add to this. This is not an end-to-end
latency measurement. The FFT engine does not guarantee unchanged timbre for
every source. TONE deliberately applies pitch-dependent filtering; select
TONE OFF when evaluating the FFT frequency response.

## Display

The waveform canvas is stored in internal RAM. A comparison copy in PSRAM lets
the display transfer only changed spans of each row. Unchanged rows and channel
labels remain on screen. Page entry and confirmation popups trigger a full
canvas redraw. The waveform refresh interval is 200 ms.

## Build and upload

Open this repository in VSCode with PlatformIO and select **lilla_fft_r2**.

```sh
platformio run -e lilla_fft_r2
platformio run -e lilla_fft_r2 -t upload
```

The build exports `LIlla_PitchShift_v_0_0_13.hex` to the repository root and
verifies that it matches the compiled file. The Upload task builds and uploads
the current sources. For a manual upload, select this HEX file in Teensy Loader.

The repository contains one firmware target, `lilla_fft_r2`:

| Path | Purpose |
| --- | --- |
| `lilla/src/`, `lilla/include/`, `lilla/lib/` | HW R2 application, UI, storage, audio graph and required components. |
| `src/`, `include/` | FFT core, spectral frames, attack cache and diagnostics. |
| `lilla/scripts/` | Firmware naming, library paths, linker placement and HEX export. |
| `tests/` | FFT core, transport, scheduling and production player adapter regressions. |
| `lilla/scripts/tests/` | Application, MIDI, storage, effects and UI regressions. |
| `.pio/` | Ignored, generated build outputs, dependencies and host test executables. |

Granular/WSOLA prototypes, offline engine comparisons, unused hardware drivers
and tests of the retired player are outside this firmware's scope. Tracked
historical sources remain available in Git history. The root contains only the
current release HEX.

## Diagnostics and validation

USB Serial at 115200 provides `FFT_TIMING`, `FFT_VOICE` and `FFT_SIGNAL` reports
covering CPU usage, voice preparation, source-read ages, allocations and signal
peaks/clipping. The diagnostic counters remain enabled in this release.

The Teensy R2 build and the following host regression suites pass:

```sh
python tests/lilla_fft_adapter_test.py
python tests/scheduling_test.py
python tests/region_controls_test.py
python -B lilla/scripts/tests/run_regressions.py
```

Host tests require Python 3.10+ and a C++20 `g++` compiler. On Windows the
runners also recognize `C:/msys64/ucrt64/bin/g++.exe`. Build the firmware first
to install the framework and decoder dependencies used by application tests.
The FFT suites run from the checked-out sources without fetching old Git commits.
See [application test instructions](lilla/scripts/tests/README.md) for filtering
and optional MP3 fixtures.

Hardware checks confirmed continuous recording with overlapping notes and
improved waveform display stability. Host tests do not measure hardware timing
or guarantee all combinations of effects fit the audio deadline.

Firmware SHA-256:
`84ff89ef7bf23d2f50e6bc917ad2d3f78eac4987285730f23592f16c6691922c`.

See [Firmware Architecture](FIRMWARE_ARCHITECTURE.md) for memory ownership and
scheduling, and [FFT Details](FFT_DETAILS.md) for the DSP design.

## License

Original LILLA PitchShift code is licensed under **GPL-3.0-or-later**.
See [LICENSE](LICENSE) for the full text and [LICENSING.md](LICENSING.md) for
the scope, retained third-party terms and firmware source-delivery requirements.
Third-party copyright and license notices remain in force; copies and the
inspected dependency versions are listed in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and [LICENSES/](LICENSES/).

The additional PJRC target-selection condition is preserved. Its compatibility
with GPLv3 distribution of the combined firmware remains an open question,
documented in [LICENSING.md](LICENSING.md#compatibility-of-the-firmware-combination).
