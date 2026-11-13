# Granular 0.6.8 - experimental FFT peak-region pitch shifting

The active engine is now a basic STFT phase vocoder, replacing WSOLA in the audio
path. The WSOLA source files, core tests and previous HEX files remain available
as a baseline. No patch/FRAM/sampler machinery is imported from Lilla.

## Source and MIDI

Startup prefers `109.raw`, configured by `ExperimentConfig::SourceFile` in
`src/ExperimentConfig.cpp`. If absent, the first catalog entry is used. Rotate
Select to choose another RAW from SPI Flash; numeric names sort numerically.
Files must be headerless mono signed 16-bit little-endian PCM at 44.1 kHz.
The catalog accepts up to 1024 names shorter than 64 bytes, with even sizes of
at least 4 bytes. The file is copied to PSRAM and shared by all six voices.
Flash is never written; FRAM is unused. This targets R1 with 16 MiB PSRAM.

The display shows selected and loaded filenames and loading/error status.
Loading starts 250 ms after the last Select step, stops all voices and reads
4096-byte chunks while continuing to service controls and MIDI. New Note Ons
are ignored during loading: release and press keys again after Ready appears.
The old sample remains allocated until loading succeeds and release tails finish.
Both files must temporarily fit in PSRAM; allocation/read failure retains the
previous sample. Startup load failure disables playback and reports an error.
Selections are not persisted across reboot.

- MIDI DIN on Serial1, 31250 baud, channel 1 only. USB Serial is diagnostics only.
- Note 60 is original pitch. Notes 36..84 map to 0.25..4; other notes are rejected.
- Pitch bend is +/-2 semitones, limited to the supported pitch range at its edges.
- Every accepted Note On starts a separate player from source sample zero.
  All notes use the same file; there is no source alternation.
- Maximum six active voices, including anticlick tails. Excess notes are rejected,
  not stolen. Repeated equal notes release oldest first; rejected-note tracking
  prevents a later Note Off from releasing an unrelated retrigger.
- Velocity scales gain. Velocity-zero Note On is Note Off. CC120/123 release all.
- LOOP_FWD over the entire source, fixed original tempo. No ADSR or sustain pedal;
  approximately 5 ms anticlick fades and gain/pitch smoothing remain.

## Controls and output

| Control | Function |
| --- | --- |
| Select rotation | Choose and load a RAW file; filename shown on display |
| LineOutVol rotation | Master volume 0..100%, 1% per step; starts at 25% |
| Step rotation | Show fixed FFT overlap: 75% |
| To rotation | Show fixed FFT size 1024 and hop 256 |
| From and all buttons | Unused in this FFT experiment |

The WSOLA antialias biquads and their Step-button bypass are not in the FFT path.
Shifted components outside the positive Nyquist band are discarded spectrally;
there is no final upward resampling stage. Spectral leakage and phase-vocoder
artifacts can still remain: this is not a claim of perfect alias rejection.

The mono mix is sum/6, duplicated to both I2S channels. The fixed headroom avoids
level changes when voices enter/leave. A solo voice is about 6.02 dB louder than
in the previous sum/12 WSOLA firmware at the same velocity and master volume.

## Engine

Each voice uses a 1024-point FFT/IFFT with periodic Hann analysis and synthesis
windows and a fixed 256-sample hop (75% overlap). The synthesis normalization is
2/3, since the sum of squared Hann windows is 1.5 at this hop. Phase differences
between analysis frames estimate instantaneous bin frequencies. Local spectral peaks
partition the spectrum into nearest-peak regions. Each region is shifted rigidly by
`(pitch - 1) * peak_frequency`, preserving its bin spacing and lobe width instead
of stretching every bin position. Peak phase accumulators use the transposed peak
frequency; surrounding bins retain their analysis phase offsets. Phase history is
stored in source-bin coordinates so overlapping output regions do not overwrite
one another's history.

Fractional shifts use two-bin linear interpolation of the centered complex spectrum,
with a phase correction for the Hann window center. Overlapping regions sum complex
values, not magnitudes. Peaks beyond Nyquist are rejected and boundary contributions
are clipped to the positive spectral band. Conjugate symmetry yields real output.
At unity pitch (note 60 with centered bend), output reads the RAW samples directly,
retaining velocity, master volume, anticlick and fixed mix headroom. The FFT stays
warm in the background, so this is an output bypass, not a CPU-saving bypass.
Leaving/entering direct output uses a 256-sample crossfade. After bend returns to
center, direct output resumes once the smoothed pitch is within 0.00002 of unity;
this tolerance accounts for float smoothing settling short of its exact target.

Transient handling uses positive spectral flux: a rise above 35% of the current
spectral magnitude sum triggers synthesis-phase reset to the mapped analysis phases.
A windowed RMS floor of 64 PCM units rejects silence; a four-hop holdoff (23.22 ms)
limits repeated resets. History and counters are separate for each voice and reset
at Note On. FFT size, hop and overlap are unchanged. This is an experimental attack
preservation measure, not phase locking; listening and CPU checks on R1 are needed.

Regions use peaks above -60 dB relative to the strongest analysis magnitude.
On initial/transient frames, phase reset includes the shift's window-center offset.
This is experimental region mapping, without formant preservation, adaptive gain,
or full temporal peak tracking. Dense/noisy signals and region changes may still
produce artifacts. Quality has priority over voice capacity: evaluate one voice
at pitch 0.5, 1, 1.5, 2 and 3 before measuring sustainable polyphony on R1.
The configured six-voice limit is retained; it is not a performance guarantee.

The FFT window spans 23.22 ms. The source is prerecorded, so frames can read ahead
without waiting for live input. For transposed notes, startup overlap builds over the first 768 output
samples (17.41 ms), in addition to the anticlick behavior; short attacks may soften.
Transport itself advances one source sample per output sample independently of pitch.
Bend is smoothed per output sample and applied spectrally at frame boundaries.

Production uses the bundled CMSIS-DSP `arm_cfft_f32` and its 1024-point table.
Host tests use a portable radix-2 FFT with the same forward/inverse normalization.
All six persistent FFT voice states, window and shared scratch arrays reside in
internal RAM. Only the source file is in PSRAM. Rendering across voices is serialized;
shared scratch is not reentrant. No allocation or logging occurs in the audio callback.

Analysis of each prerecorded frame runs 128 samples before its synthesis deadline.
The analysis magnitudes, phases and unshifted frequencies remain in per-voice RAM;
mapping and synthesis use the actual pitch at the deadline, so bends received in
between still take effect. After startup, analysis and synthesis alternate across
128-sample callbacks without changing the source position or adding output latency.
The first frame still executes both stages immediately: simultaneous new notes can
still cause a startup peak. This distributes work; it does not reduce the FFT count.
Timing groups with no generated hop can now include an analysis stage.

## Serial reports and performance

- `NOTE_ON pitch=...`: ratio requested for each accepted note, including bend.
- Every two seconds: `PLAYERS n=... cost_us[avg/max]=... cost_pct[avg/max]=...`.
  Cost sums active renderers in each 128-sample block, including FFT frames and
  startup. It excludes mixing, reference metering and output delivery.
- `n` is the instantaneous active count; timing describes the preceding interval.
  After releasing all notes, n=0 can accompany nonzero costs for one report.
- Rotating Step/To prints the fixed FFT geometry; Note Off and volume changes are silent.
- Startup/source errors and rejected notes are reported. The 16 KiB logging ring is
  nonblocking and drops complete lines if full.

The block budget is about 2902.49 us at 44.1 kHz. Six simultaneous voices can require
six FFT/IFFT pairs in the first callback. Subsequent analysis and synthesis stages
are spread across alternating callbacks. Six is the configured capacity, not a measured
real-time guarantee. Test one through six sustained voices and simultaneous attacks
on R1. Keep room below 100% renderer cost for callback overhead. Host timings do not
predict Teensy performance; CMSIS execution and analog audio have not been bench-tested here.

## Build and tests

Build: `C:/Users/Utente/.platformio/penv/Scripts/platformio.exe run`.
Firmware: `.pio/build/teensy41/firmware.hex`, exported as `Granular_v0_6_8.hex`.
No upload is performed. Load with Teensy Loader to test the actual board.

```powershell
g++ -std=c++20 -O2 -Wall -Wextra -Werror -static -I include src/FftPitchCore.cpp tests/fft_test.cpp -o .pio/fft_test.exe
./.pio/fft_test.exe
g++ -std=c++20 -O2 -Wall -Wextra -Werror -static -I tests/stubs -I include src/FftPitchCore.cpp src/GranularAudio.cpp tests/two_voice_test.cpp -o .pio/two_voice_test.exe
./.pio/two_voice_test.exe
g++ -std=c++20 -O2 -Wall -Wextra -Werror -static -I tests/stubs -I include src/FftPitchCore.cpp src/GranularAudio.cpp src/NoteVoices.cpp src/ExperimentConfig.cpp tests/midi_voices_test.cpp -o .pio/midi_voices_test.exe
./.pio/midi_voices_test.exe
g++ -std=c++20 -O2 -Wall -Wextra -Werror -static -I tests/midi_stubs -I include -I C:/Users/Utente/.platformio/packages/framework-arduinoteensy/libraries/MIDI/src src/MidiInput.cpp tests/midi_input_test.cpp -o .pio/midi_input_test.exe
./.pio/midi_input_test.exe
python tests/encoder_test.py
python tests/scheduling_test.py
python tests/region_quality_test.py
```

Tests check tone transposition at 0.25..4, neutral reconstruction, wrapping, fixed
tempo, out-of-band rejection, bend, clean restart, six-slot allocation, repeated
notes, exact mixing of independent FFT cores, dropped output allocation and headroom.
The scheduling regression compares the new engine against its synchronous-analysis
variant: six interleaved voices match exactly across attacks, loops and bends.
It also checks alternating stage execution, direct RAW output and return from bend.
The region-quality regression compares tones against 0.6.7 (commit 0bb9abb),
checking target-frequency energy and level at pitch 0.5, 1, 1.5, 2 and 3.
At pitch 3, tested 220/997 Hz tones concentrate over 99.99% of energy at the target,
versus approximately 87.2%/85.7% previously. These synthetic measurements do not
establish speech quality or CPU cost. The quality comparison needs local Git history.
The mixer clock/UART are mocked. The old standalone WSOLA tests can still be built
with `src/WsolaCore.cpp src/WsolaAntialias.cpp`; they no longer exercise the live mixer.

Teensy 4.1 at 600 MHz; Audio Board Rev D, I2S/SGTL5000, Flash CS6. SPI1 controls
retain the Lilla wiring. Line-out level is 13 and headphone level 0.8. No effects,
recording, FRAM access or configuration persistence. The ILI9341 display setup
and text-clearing pattern are adapted from WHOLE_LILLA, keeping the FFT engine.
Display SPI1: CS38, DC29, RST30, MOSI26, SCK27; rotation 1. Select uses MCP chip 3
pins b5/b4. Display and encoder behavior still require verification on R1.

## Startup investigation (0.6.6 diagnostic)

The six-voice 0.6.5 build was reported unresponsive with `109.raw` loaded and
Ready on screen, even before playing notes. The cause is not yet established.
`Granular_v0_6_6_diagnostic.hex` follows a full clean build and adds visible
Starting MIDI / Starting audio / Ready - loop running stages and BOOT serial
messages. It prints Teensy CrashReport when available after a fault/reboot.
First-note breadcrumbs identify MIDI entry/exit, source pointer/count, forward
FFT stages (100..102) and inverse FFT stages (200..201). This is a diagnostic
build, not a verified fix. Voice capacity and DSP processing are unchanged.

## Previous engine: phase-update optimization (0.6.7)

Resolve the phase-lock anchors before advancing phases. Only phases actually
referenced by the output are advanced; independent phases that locking would
immediately overwrite are not computed. All referenced phases are still evaluated
before synthesis history is changed, preserving traversal independence. FFT size,
overlap, transient detection, six voices and direct unity output remain unchanged.
Host regression matches the pre-optimization output sample for sample with six
voices and changing pitches. CPU savings must be measured on R1; no percentage
improvement is claimed from host tests.

PLAYERS now also prints `callback_pct[avg/max]`, covering the measured audio callback
including renders, reference metering, mixing and output delivery. As before,
interval statistics only include callbacks with active voices; the final mix
statistics bookkeeping after the timer read is excluded. Compare this maximum
against 100% when evaluating the actual six-voice margin.
The working 0.6.6 startup diagnostics and crash breadcrumbs are retained. Firmware
is rebuilt from clean objects. The previous startup failure's cause remains unproven.

## Verified recovery of 0.6.8

The active sources have been restored to the hardware-tested 0.6.8 configuration:
FFT 1024, hop 256, region mapping, six slots and sum/6 mixing. Use at most four
notes for the current quality experiments; the slot count remains six to reproduce
the original firmware exactly. A clean build produced identical Intel HEX records
to the original Granular_v0_6_8.hex (line endings ignored). Its SHA-256 is
`cfa2a63631067c5fb44eb9acc62e76af501ffa6fa83d4ca475cf7be73d63dad0`.
The original HEX was not overwritten. Versions 0.6.9 and 0.6.10 are retained only
as rejected experiments; neither is the current firmware. This recovery does not
establish why 0.6.10 failed on the board.

Run `python tests/verify_068_build.py` after a clean build to check the exact match.
This is a recovery check, not a test intended to pass after future firmware changes.
Pre-recovery working files were backed up in `.pio/recovery/pre_068_restore.zip`;
that local archive is ignored by Git and includes the user's AGENTS.md changes,
which were left untouched in the working tree.
