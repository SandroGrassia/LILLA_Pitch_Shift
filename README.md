# LILLA_PitchShift v0_0_13

LILLA_PitchShift runs the complete Lilla application on **HW R2** with a dual-band
FFT pitch-shift engine. Pitch changes preserve the source transport tempo.
The firmware provides **two physical playback voices**, shared by keyboard
performance, MIDI Loop, preview and Live Sampler, including release tails.

See [Firmware Architecture](FIRMWARE_ARCHITECTURE.md) for component responsibilities,
DSP scheduling, memory ownership and source handling.

For windowing, phase locking, reconstruction and illustrated examples, see
[FFT Details](FFT_DETAILS.md).

## Hardware and build target

- Teensy 4.1 at 600 MHz; Arduino framework and Teensy Audio library.
- HW R2: 256 KB FRAM and 32 MB PSRAM.
- Default PlatformIO environment: `lilla_fft_r2`.
- Audio blocks: 128 samples at approximately 44.1 kHz, one block every 2.90 ms.
- Application: `lilla/src/` and `lilla/lib/`; shared DSP: `include/` and `src/`.

## Playback

A patch contains up to eight Instrument slots. For a note matching multiple
Instruments, only the first is played: Instrument layering is disabled.
Two slots are the maximum simultaneous voice allocation, not a guarantee that
all effect and source configurations fit the audio deadline.

Base note transposition is limited to **-24..+12 semitones** relative to the
Instrument root key. Notes outside this range are rejected. Note and preset
tuning combine within pitch 0.25..2; pitch bend adds +/-2 semitones. The final
ratio, including modulation, is bounded to approximately **0.222725..2.244924**.

The six modes are ONCE_FWD, ONCE_REV, LOOP_FWD, LOOP_REV, LOOP_FWD_REV and
LOOP_REV_FWD. Region limits, permitted mode transitions and cross-mix edits are
handled by the engine. The minimum selectable region is 100 samples. Ping-pong
modes do not use the end/start cross-mix.

At unity pitch, output comes directly from the source while FFT histories remain
active for subsequent pitch changes. This bypass does not eliminate FFT CPU cost.
ADSR, velocity, volume, pan, TONE, VCF/LFO, resolution/downsampling, routing and
delay remain part of Lilla's audio chain.

**TONE is an intentional pitch-dependent low-pass filter.** With a nonzero TONE
setting, its cutoff falls as pitch rises above 1. TONE OFF excludes this filter;
use OFF when evaluating the FFT engine's own frequency response.

## Dual-band FFT

The high branch uses FFT 1024/hop 256 at the original sample rate. The low branch
filters with a 65-tap FIR, decimates by four and uses FFT 1024/hop 256 at 11.025 kHz.
It therefore observes about 4096 original samples. The two branches blend over
200..400 Hz of the input spectrum; cubic interpolation restores the low branch
to the output rate. Phase locking and transient handling operate within the engine.

Analyses keep advancing throughout playback. Initial cached spectra describe only
the attack, not the whole file. The low branch improves separation of low-frequency
components, but complex noisy/percussive material can still lose clarity when
transposed upward. The FFT does not promise unchanged timbre for every source.

## Attack preparation and cache

Two shared RAM cache entries hold pitch-independent initial analyses of both
bands. They are **two attacks in total**, not two per voice or per saved patch.
Any number of successive notes can reuse an entry; simultaneous voices retain
independent phase, overlap and envelope state.

Entries are keyed by source pointer/range, region bounds, play mode and cross-mix.
Pitch, velocity and gain do not change the key. A patch may use more than two
file/geometries: an uncached attack uses staged preparation instead.

The audio clock prepares the first two eligible active PSRAM presets only while
all players are Idle and no sample copy is pending. Leave release tails to finish
after loading or editing to allow preparation. Sample memory replacement or reuse
invalidates the cache. The cache is temporary and is not saved with the patch.

A hit skips the six preparation callbacks. A miss performs four FIR portions,
one low-band analysis and one high-band analysis before rendering. Transport and
ADSR attack wait during preparation. This adds approximately **17.4..20.3 ms** of
preparation/scheduling wait; it is not the complete measured MIDI-to-output latency.
Cache hits still require synthesis and may wait for the assigned startup turn.
Live Sampler does not use the attack cache. Steady-state FFT work is unchanged
by whether the attack was cached.

## Sources and Live Sampler

Stored samples are intended to play from PSRAM. The implementation retains a
Flash reader, but the current FFT source-read estimate exceeds the 1800 us
admission limit even for an ordinary Flash note: direct Flash playback is
therefore effectively rejected until a usable PSRAM source is available.
Patch loading may also explicitly stop voices while storage is prepared.

Live Sampler uses the same FFT engine with a circular PSRAM source reader.
Unrecorded first-fill samples read as zero, and playback waits when the required
source region is unavailable. Moving forward starts require 5120 recorded samples
(about 116 ms) of source margin. Startup preparation and I/O buffering add latency;
116 ms is not an end-to-end measurement. Recording-stop handling drains the
remaining recorded material in forward once playback.

## Timing diagnostics

With USB Serial connected at 115200, `FFT_TIMING` reports approximately every two
seconds when transmit space is available.

| Field | Meaning |
| --- | --- |
| `ms` | Actual elapsed reporting interval; it can exceed 2000 ms. |
| `active` | Active players at the snapshot, including tails; not the interval maximum. |
| `cpu_max` | Maximum Teensy audio CPU usage in the reporting interval. |
| `start_max_us` | Maximum per-player preparation/first-block processing time. |
| `run_max_us` | Maximum per-player steady rendering time, including effects but excluding final delivery/cleanup. |
| `starts` | First rendered blocks in the interval; not the number of preparation steps. |
| `deadline_stops` | Emergency voice stops since the preceding report. |
| `cache_hits` | Successful attack restorations, cumulative since boot. |

The audio block budget is about 2902 us. A peak over 100% or a nonzero
`deadline_stops` indicates insufficient timing margin. Cache hits reduce startup
preparation only; they do not guarantee that sustained notes fit the deadline.

## Build, firmware and validation

```sh
platformio run -e lilla_fft_r2
python tests/region_controls_test.py
python tests/lilla_fft_adapter_test.py
```

The version is declared in `lilla/lib/config/config.h`. The build exports
**`LILLA_PitchShift_v0_0_13.hex`** to the repository root and verifies it against
`.pio/build/lilla_fft_r2/LILLA_PitchShift_v0_0_13.hex`. When uploading, explicitly
select that verified root file so Teensy Loader displays the release name.
Compilation does not automatically upload it.

Host adapter/cache tests cover source equivalence, play modes, cache keys,
initial-analysis reuse and simulated live-buffer transitions. They do not replace
hardware checks of effects, timing, storage and audible continuity.

## Release v0_0_13 recap (2026-11-19)

This final release consolidates the work tested in diagnostic versions
v0_0_13_diagnostic through v0_0_23_diagnostic. Those archived filenames retain
their original numbering. The final LILLA_PitchShift_v0_0_13.hex is a new build
and is distinct from the earlier v0_0_13_diagnostic.hex. FFT serial diagnostics
remain enabled; this release renaming does not change audio or display behavior.

- Input capture: AudioInputI2S runs before the FFT players so completed DMA input
  blocks are exchanged before the next half-block deadline. The user confirmed
  this resolved the recording-time disturbance with overlapping notes.
- DSP memory: each spectral frame owns its FFT and scratch storage. Separation
  alone did not resolve the disturbance; input scheduling was the decisive fix.
- Attack cache: two reusable initial analyses for stable PSRAM sources, prepared
  only while voices are idle, with invalidation before source storage reuse.
- Diagnostics: FFT_TIMING, FFT_VOICE and FFT_SIGNAL report timing, allocation,
  source-read ages, preparation waits, signal peaks and clipping.
- Live envelope: release is 1 second; decay remains 2 seconds.
- Canvas storage: fixed 310 x 97 RGB565 buffer in RAM2; the idle attack-cache
  builder resides in PSRAM to recover internal memory.
- Waveform alignment: initialized on page entry and re-anchored on window,
  channel or buffer changes. The user reported reduced flicker with v0.0.22.
- Display updates: compare against a previous-frame copy in PSRAM and transfer
  only changed row spans. Page entry and confirmation popups force a full repaint;
  unchanged channel labels are not redrawn. The refresh interval remains 200 ms.
  The user gave positive feedback; no quantitative display timing was measured.

The final v0.0.13 build reports 21,152 bytes of RAM1 free for local variables
and 73,824 bytes of RAM2 free for dynamic allocation. The six private FFT
transform allocations consume another 49,320 heap bytes plus allocator overhead
at startup; linker figures therefore do not describe the remaining runtime heap.

Final release validation:

- `platformio run -e lilla_fft_r2`: passed.
- `python tests/lilla_fft_adapter_test.py`: passed.
- `python tests/scheduling_test.py`: passed.
- `python tests/region_controls_test.py`: passed.
- Exported HEX matches the compiled artifact byte for byte.
- SHA-256: `84ff89ef7bf23d2f50e6bc917ad2d3f78eac4987285730f23592f16c6691922c`.
