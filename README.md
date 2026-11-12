# Granular 0.6.3 - four-voice FFT pitch experiment

The active engine is now a basic STFT phase vocoder, replacing WSOLA in the audio
path. The WSOLA source files, core tests and previous HEX files remain available
as a baseline. No patch/FRAM/sampler machinery is imported from Lilla.

## Source and MIDI

Startup prefers `109.raw`, configured by `ExperimentConfig::SourceFile` in
`src/ExperimentConfig.cpp`. If absent, the first catalog entry is used. Rotate
Select to choose another RAW from SPI Flash; numeric names sort numerically.
Files must be headerless mono signed 16-bit little-endian PCM at 44.1 kHz.
The catalog accepts up to 1024 names shorter than 64 bytes, with even sizes of
at least 4 bytes. The file is copied to PSRAM and shared by all four voices.
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
- Maximum four active voices, including anticlick tails. Excess notes are rejected,
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

The mono mix is sum/4, duplicated to both I2S channels. The fixed headroom avoids
level changes when voices enter/leave. A solo voice is about 9.54 dB louder than
in the previous sum/12 WSOLA firmware at the same velocity and master volume.

## Engine

Each voice uses a 1024-point FFT/IFFT with periodic Hann analysis and synthesis
windows and a fixed 256-sample hop (75% overlap). The synthesis normalization is
2/3, since the sum of squared Hann windows is 1.5 at this hop. Phase differences
between analysis frames estimate instantaneous bin frequencies. Frequencies are
multiplied by pitch; magnitudes are mapped to nearest destination bins. Collisions
sum magnitudes and use magnitude-weighted frequency. Output phases accumulate the
shifted frequencies, then conjugate symmetry produces a real IFFT output.
At exact unity pitch, analysis phases are preserved directly for reconstruction.

Transient handling uses positive spectral flux: a rise above 35% of the current
spectral magnitude sum triggers synthesis-phase reset to the mapped analysis phases.
A windowed RMS floor of 64 PCM units rejects silence; a four-hop holdoff (23.22 ms)
limits repeated resets. History and counters are separate for each voice and reset
at Note On. FFT size, hop and overlap are unchanged. This is an experimental attack
preservation measure, not phase locking; listening and CPU checks on R1 are needed.

Identity phase locking ties each mapped bin to the nearest analysis-spectrum peak,
preserving its analysis phase offset from that peak. Peaks below -60 dB relative
to the strongest magnitude are ignored. Peak phases advance first; phase locking
then uses that snapshot to avoid traversal-order dependence. Missing/out-of-band
or collision-displaced anchors fall back to independent phase advancement.
Unity pitch and transient resets retain the direct analysis-phase behavior.
Nearest-bin magnitude mapping remains unchanged; this is not formant preservation.

This is a basic phase vocoder: no
formant preservation or loudness compensation. Expect level differences with pitch,
possible phasiness, softened attacks and artifacts on complex signals. These must
be evaluated against WSOLA on the actual source, not inferred from tone tests.

The FFT window spans 23.22 ms. The source is prerecorded, so frames can read ahead
without waiting for live input. Startup overlap builds over the first 768 output
samples (17.41 ms), in addition to the anticlick behavior; short attacks may soften.
Transport itself advances one source sample per output sample independently of pitch.
Bend is smoothed per output sample and applied spectrally at frame boundaries.

Production uses the bundled CMSIS-DSP `arm_cfft_f32` and its 1024-point table.
Host tests use a portable radix-2 FFT with the same forward/inverse normalization.
All four persistent FFT voice states, window and shared scratch arrays reside in
internal RAM. Only the source file is in PSRAM. Rendering across voices is serialized;
shared scratch is not reentrant. No allocation or logging occurs in the audio callback.

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

The block budget is about 2902.49 us at 44.1 kHz. Four simultaneous voices can require
four FFT/IFFT pairs in one callback. Four is the configured capacity, not a measured
real-time guarantee. Test one through four sustained voices and simultaneous attacks
on R1. Keep room below 100% renderer cost for callback overhead. Host timings do not
predict Teensy performance; CMSIS execution and analog audio have not been bench-tested here.

## Build and tests

Build: `C:/Users/Utente/.platformio/penv/Scripts/platformio.exe run`.
Firmware: `.pio/build/teensy41/firmware.hex`, exported as `Granular_v0_6_3.hex`.
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
```

Tests check tone transposition at 0.25..4, neutral reconstruction, wrapping, fixed
tempo, out-of-band rejection, bend, clean restart, four-slot allocation, repeated
notes, exact mixing of independent FFT cores, dropped output allocation and headroom.
The mixer clock/UART are mocked. The old standalone WSOLA tests can still be built
with `src/WsolaCore.cpp src/WsolaAntialias.cpp`; they no longer exercise the live mixer.

Teensy 4.1 at 600 MHz; Audio Board Rev D, I2S/SGTL5000, Flash CS6. SPI1 controls
retain the Lilla wiring. Line-out level is 13 and headphone level 0.8. No effects,
recording, FRAM access or configuration persistence. The ILI9341 display setup
and text-clearing pattern are adapted from WHOLE_LILLA, keeping the FFT engine.
Display SPI1: CS38, DC29, RST30, MOSI26, SCK27; rotation 1. Select uses MCP chip 3
pins b5/b4. Display and encoder behavior still require verification on R1.
