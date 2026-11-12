# Granular 0.5.6 - MIDI WSOLA experiment

Standalone PlatformIO laboratory for pitch change on Lilla R1 hardware
(Teensy 4.1, 16 MiB PSRAM, 128 KiB FRAM). FRAM is not used. The active engine is
WSOLA followed by resampling. The previous GranularCore and its tests remain as
an inactive baseline. No Lilla patch, sampler, MIDI-loop or storage manager is imported.

## Playing and controls

MIDI DIN uses Serial1 at 31250 baud, the same transport as Lilla, on channel 1 only.
USB Serial is output-only diagnostics, not a MIDI input. MIDI Thru is disabled.

- Note 60 plays the source at its original pitch. Notes 36..84 map to pitch 0.25..4.
  Other notes are rejected and reported. Tempo remains 1x.
- Pitch bend is +/-2 semitones and affects held notes, anticlick tails and new notes.
  At the ends of the supported range the ratio is clamped to 0.25..4.
- Each accepted Note On starts a separate player at source sample zero, alternating
  A, B, A, B. Velocity scales gain by velocity/127; zero velocity is Note Off.
- Each player loops forward through its entire source until Note Off (LOOP_FWD).
- Up to 12 voices, including anticlick tails. At capacity, new notes are rejected;
  there is no voice stealing. Rejected events do not advance the A/B alternation.
- Repeated equal notes occupy different players. Note Off releases the oldest held
  instance of that key. After a rejected Note On, further Note Ons of that key are
  rejected until its outstanding Note Offs have been received, preserving ownership.
- CC120 and CC123 release all held voices using the same anticlick fade and clear
  rejected-note tracking. Sustain, program changes and other CCs are not implemented.
- No ADSR is applied. The existing approximately 5 ms start/stop anticlick fades,
  pitch smoothing and gain smoothing remain. A reused inactive voice starts fresh.

| Encoder | Rotation | Pushbutton |
| --- | --- | --- |
| LineOutVol | Master volume 0..100%, 1% per step | Unused |
| Step | Show fixed overlap: 50% | Toggle antialias ON/BYPASS |
| To | Show fixed grain: 882 samples | Unused |
| From | Unused | Unused |

Startup: no notes, grain 20 ms (882 samples), overlap 50%, volume 25%, centered bend.
In 0.5.5 the firmware fixes the grain at 882 samples and overlap at 50%.
To and Step rotations only display those fixed values. The core still supports
variable geometry for regression tests, but the hardware controls do not change it. Search remains +/-441
source samples (about 10 ms). Geometry changes fade out and rebuild at the current
transport, then fade back in; they do not restart the source.

The mono output is the sum of all voices divided by 12, duplicated to both I2S
channels. This fixed headroom prevents overflow without gain jumps when notes enter
or leave. One solo voice is attenuated by about 21.6 dB before master and velocity
gain; at master 25% and velocity 127 its attenuation is about 33.6 dB.

## Sources and R1 memory

Edit the two names in `src/ExperimentConfig.cpp` when the experiment files are known.
Both slots currently use `0.raw`, preserving the existing local source selection.
Identical filenames share one allocation: A/B alternation is implemented, but the
two slots sound identical until two different files are configured.

Files must already exist in SPI Flash as headerless mono signed 16-bit little-endian
PCM at 44.1 kHz. Startup copies each distinct file once into PSRAM. No Flash writes
or source recordings are included. Missing files, invalid sizes or insufficient
memory disable playback and report the cause. Allocations are freed on load failure.

All 12 WsolaCore instances and their persistent working buffers are allocated and
constructed in PSRAM before playback; their total state is approximately 0.69 MiB.
The temporary search and correlation-reference buffers are shared in internal DTCM
RAM (about 21 KiB total), independently of the PSRAM allocation of the cores.
The exact size is printed at startup. This avoids exceeding internal RAM with 12
large windows. Sources and voice states must fit the available PSRAM together.
The firmware uses the detected PSRAM pool, not an assumed 32 MiB R2 capacity.
There is no allocation in the audio callback. Mix buffers, search/correlation scratch and diagnostics remain
in internal RAM. PSRAM traffic/cache effects are part of the measured engine cost.

## WSOLA behavior

The synthesis hop is window minus the integer overlap length. WSOLA aligns the
next source fragment with the pending tail using mean-removed normalized
cross-correlation near the nominal transport. Search uses coarse candidates every
8 samples and a fine pass around the best candidate, with decimated correlation.
The nominal source timeline advances one sample per output sample; local alignment
offsets do not accumulate into tempo drift. At pitch 1 the nominal match is used.

The 50% path uses the original complementary linear crossfade. Other overlaps use
normalized weighted accumulation; 75% retains up to four overlapping contributions.
The selected source window is also the intermediate WSOLA window before resampling:
its approximate output duration is window/pitch. It is not a fixed output-grain
length. Profiles were not changed in this version.

There is no transient detector or formant preservation. Complex/percussive sources can expose WSOLA limitations.
Cyclic source endpoints have no separate crossfade. Hardware listening is required.

## Antialias experiment (0.5.1)

Antialias is ON at startup. Press Step to compare ON/BYPASS; rotation shows the
fixed overlap in the current experiment. A press prints `AA=ON` or `AA=BYPASS`. Compare separate two-second
intervals with fixed pitch, player count and parameters.

Two biquads form a fourth-order Butterworth lowpass with unity DC gain. They process
the continuous intermediate WSOLA stream before interpolation/resampling, including
all samples skipped when pitch exceeds 1. Filter state persists across hop boundaries;
correlation and overlap accumulation continue to use the unfiltered source.
There is no new audio buffer or heap allocation.

The target -3 dB cutoff is 0.45/max(1,pitch) cycles per intermediate sample, or about
19845/pitch Hz at 44.1 kHz (9922.5 Hz at pitch 2, 4961.25 Hz at pitch 4). This leaves
a transition band before the theoretical 22050/pitch Hz limit. It is a moderate
filter, not complete alias rejection at the folding boundary.

At pitch <=1 the settled path is sample-exact with bypass. The wet contribution
increases from 0 to 1 over pitch 1..1.05; manual bypass crossfades over about 5 ms.
Coefficients follow the larger of current smoothed pitch and target pitch, with
coefficient smoothing over intermediate samples. The current-pitch check runs every
64 filtered output samples; new pitch commands also update the target coefficients.
There is no trigonometric calculation per audio sample. Note starts and geometry
rebuilds reset the filter state while the existing anticlick gate is closed.

Host tests of the complete WSOLA/resampling path reduce output alias RMS by about
32 dB for a 16 kHz input at pitch 2, 30 dB for 10 kHz at pitch 4, and 56 dB for
20 kHz at pitch 1.5, across all four overlaps. These are selected synthetic tones,
not a guarantee for speech. Test consonants on one voice, toggling
Step without changing grain/overlap. Avoid audio overruns during listening. The filter does not repair WSOLA transient
alignment or CPU overruns. Its added CPU cost is included in the reported player timings
and must be measured on R1. An IIR filter can overshoot input peaks on transients;
the existing final sample saturation and fixed mixer headroom remain.

## Shared WSOLA workspace (0.5.3)

The search region (6174 int16 samples) and the correlation reference (4410 int16
samples) are static, 32-byte-aligned arrays in the normal internal `.bss` section.
On Teensy 4.1 this is DTCM, not the external PSRAM used for each WsolaCore object.
The shared workspace occupies 21168 bytes before alignment padding. Removing the
per-voice search array saves 12348 bytes per voice, 148176 bytes for 12 voices.

Each Generate fills the search region and reference before scoring candidates.
For 50% overlap the persistent tail is copied once into the internal reference;
other overlaps derive the normalized reference directly from that voice's pending
accumulation. All repeated candidate comparisons then read internal RAM. Persistent
tails, accumulated overlaps, output hops and filter state remain independent per voice.

Render/Generate calls across all instances must be serialized, as in the audio
callback. The workspace is not reentrant and must not be used by concurrent renders
from another interrupt or thread. No lock, allocation, algorithm parameter or search
resolution was added. Scratch is never retained as voice state between calls.

The before/after host comparison produced identical samples and state for 12 voices
with different sources, all overlaps, short/long grains, pitch/bypass changes and
stop/restart. A separate per-block versus interleaved-render test checks isolation.
The optimization must be measured on R1 with the same notes, grain, overlap and AA
setting as 0.5.2; host timing cannot predict the DTCM/PSRAM speedup or safe polyphony.

## Reduced correlation sampling (0.5.4)

The coarse/fine correlation stride targets are reduced from 128/256 to 64/128.
Stride still uses max(1, overlap/target) with integer division, so these are nominal
sample counts, not strict limits. With the startup window of 882 and 50% overlap
(441 samples), each coarse score compares 74 samples instead of 147; each fine
score compares 147 instead of 441. The final correlation diagnostic uses the same
reduced fine stride. Short overlaps can retain the original sample density.

Candidate positions, search radius, normalization, distance penalty, grain length,
overlap and antialias processing are unchanged. The shared DTCM workspace remains.
No reference-statistic caching is included in this experiment. The selected match
and audible result may differ because the comparison uses less information.
Compare 0.5.3 and 0.5.4 with identical notes and controls, watching cost_us[avg/max]
and listening for alignment artifacts. Any CPU/polyphony improvement requires R1
measurements; the host regressions do not establish performance or speech quality.

## Fixed geometry and per-candidate statistics (0.5.6)

Grain remains fixed at 882 source samples (20 ms), overlap at 50% and hop at 441.
Rotating Step or To reports the fixed value without changing audio parameters.
Volume and the Step antialias button remain active. Correlation sampling remains
at nominal 64/128, with shared DTCM search/reference buffers and the same filter.

The experimental reference-statistic cache from 0.5.5 has been removed because it
provided no appreciable measured benefit on R1. Score again computes reference and
candidate sums, energies and sample count for every candidate, as in 0.5.4. Search
positions, normalization and the scoring formula are unchanged. No other DSP or
control behavior is reverted.

## Serial reports (0.5.2, controls fixed in 0.5.5)

The regular output is intentionally compact:

```text
NOTE_ON pitch=1.25992
PLAYERS n=3 cost_us[avg/max]=1149.98/4285.17 cost_pct[avg/max]=39.6/147.6
OVERLAP=50% (fixed)
GRAIN samples=882 (fixed)
```

- Every accepted Note On prints its target pitch ratio, including the current bend.
- Every two seconds, `PLAYERS n` counts currently active players, including anticlick
  tails. `cost_us[avg/max]` is the mean/maximum summed rendering time of active
  players per 128-sample audio block during the interval. It includes startup and
  generation, but excludes idle slots, metering, mixing and output delivery.
- `cost_pct[avg/max]` expresses those two costs as percentages of the approximately
  2902.49 us block budget. The maximum is the largest simultaneous sum in a block,
  not the sum of independent per-player maxima. Total callback cost is higher than
  this renderer-only cost; a value below 100% does not guarantee no overruns.
- The mean uses only blocks containing an active voice. The count is instantaneous,
  while the costs describe the interval: after the last Note Off, `n=0` may briefly
  accompany nonzero costs. An entirely idle interval reports zero costs.
- Rotating Step prints `OVERLAP=50% (fixed)` without changing the overlap.
- Rotating To prints `GRAIN samples=882 (fixed)` without changing the grain.
- Pressing Step still reports `AA=ON` or `AA=BYPASS` for the listening comparison.
  Volume changes and Note Offs do not print a report. Startup information, errors
  and rejected-note messages remain available.

Printing is in the foreground with audio interrupts enabled, through a nonblocking
16 KiB ring. A full ring drops complete lines. The MIDI collector preserves order,
extends Serial1 RX by 512 bytes and bounds each parsing pass to 64 entry-snapshot
bytes, 8 messages and 80 us. Unconsumed bytes remain for the next loop; the time
limit excludes note processing. Host tests cannot establish hardware throughput.

## Hardware and validation

Teensy 4.1 at 600 MHz, Audio Board Rev D with I2S/SGTL5000. Line-out level 13,
headphone level 0.8. SPI Flash CS 6. Controls use SPI1 MOSI 26, SCK 27, MISO 39,
CS 37; display CS 38 stays high. MCP mapping is unchanged from the prior firmware.
No effects, display UI, recording, FRAM access or configuration persistence.

Twelve slots are an experimental ceiling, not a guarantee of real-time polyphony.
Measure 1, 2, 4, 8 and 12 held notes, both simultaneous and staggered, using different
pitches and both sources. This firmware uses fixed 20 ms grains and 50% overlap;
older variable-geometry experiments remain available in the previous HEX files. Hold the same notes and settings for complete two-second reporting intervals.
Compare player cost maxima and audible artifacts; the compact report excludes
callback overhead, so leave headroom below the block budget. Older timings from the two-voice/internal-RAM firmware do not apply.

Build with `pio run`; if absent from PATH, use
`C:/Users/Utente/.platformio/penv/Scripts/platformio.exe run`.
Firmware: `.pio/build/teensy41/firmware.hex`, exported as `Granular_v0_5_6.hex`.
No upload is performed; load the HEX with Teensy Loader for physical tests.

Host checks (GCC; `-static` avoids Windows runtime DLL mismatches):

```powershell
g++ -std=c++20 -O2 -Wall -Wextra -Werror -static -I include src/WsolaCore.cpp src/WsolaAntialias.cpp tests/wsola_test.cpp -o .pio/wsola_test.exe
./.pio/wsola_test.exe
g++ -std=c++20 -O2 -Wall -Wextra -Werror -static -I include src/WsolaCore.cpp src/WsolaAntialias.cpp tests/overlap_test.cpp -o .pio/overlap_test.exe
./.pio/overlap_test.exe
g++ -std=c++20 -O2 -Wall -Wextra -Werror -static -I tests/stubs -I include src/WsolaCore.cpp src/WsolaAntialias.cpp src/GranularAudio.cpp tests/two_voice_test.cpp -o .pio/two_voice_test.exe
./.pio/two_voice_test.exe
g++ -std=c++20 -O2 -Wall -Wextra -Werror -static -I tests/stubs -I include src/WsolaCore.cpp src/WsolaAntialias.cpp src/GranularAudio.cpp src/NoteVoices.cpp src/ExperimentConfig.cpp tests/midi_voices_test.cpp -o .pio/midi_voices_test.exe
./.pio/midi_voices_test.exe
g++ -std=c++20 -O2 -Wall -Wextra -Werror -static -I tests/midi_stubs -I include -I C:/Users/Utente/.platformio/packages/framework-arduinoteensy/libraries/MIDI/src src/MidiInput.cpp tests/midi_input_test.cpp -o .pio/midi_input_test.exe
./.pio/midi_input_test.exe
g++ -std=c++20 -O2 -Wall -Wextra -Werror -static -I include src/WsolaCore.cpp src/WsolaAntialias.cpp tests/antialias_test.cpp -o .pio/antialias_test.exe
./.pio/antialias_test.exe
g++ -std=c++20 -O2 -Wall -Wextra -Werror -static -I include src/WsolaCore.cpp src/WsolaAntialias.cpp tests/wsola_workspace_test.cpp -o .pio/wsola_workspace_test.exe
./.pio/wsola_workspace_test.exe
python tests/encoder_test.py
```

The MIDI input test uses the actual bundled MIDI library with a mocked UART/clock.
Voice tests cover 12 simultaneous voices, alternating distinct sources, repeated
notes, rejection/NoteOff ownership, release tails, fresh restart, bend, geometry and
independent statistics. The older two-voice test now checks its two active voices
inside the 12-slot mixer, plus coincident full-scale output from all 12 slots.
Clock values are mocked: these tests establish behavior, not CPU headroom or wiring.

## Provenance

Lilla hardware mapping and encoder basis: Sandro Grassia's firmware, source commit
283739101236e5ebba9e0c3b307b56c8177b23e9. MIDI transport and bounded receive approach
follow WHOLE_LILLA without importing its patch/player/loop dependencies.
WSOLA is implemented here without an external processing library. Teensy Audio,
Arduino MIDI, SerialFlash and Adafruit libraries retain their own licenses.
