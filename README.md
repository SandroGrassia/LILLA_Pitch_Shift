# Granular 0.2.4 - WSOLA experiment

Standalone PlatformIO firmware for the existing Lilla hardware. The active engine
uses waveform-similarity overlap-add (WSOLA) followed by resampling, replacing the
uniformly scheduled granular engine from version 0.1.2. The previous GranularCore
and its tests remain in the repository as a baseline; they are not connected to audio.

## Controls

| Encoder | Rotation | Pushbutton |
| --- | --- | --- |
| Step | Pitch 0.25 to 4, in 10-cent steps | Start/stop |
| From | Analysis window: 5, 10, 15, 20, 25, 30, 40, 50, 100 ms | Unused |
| To | Alignment search radius: 0, 1, 2, 5, 10, 15, 20 ms | Unused |
| LineOutVol | Output gain: 0 to 100 percent | Unused |

Startup is stopped, pitch 1, window 20 ms, search +/-10 ms, volume 25 percent.
Overlap is fixed at 50 percent, with complementary linear ramps. Grain-count and
slope controls from 0.1.2 are replaced by the WSOLA geometry and search control.
Search zero disables alignment and provides a direct listening comparison.

Window duration refers to the intermediate WSOLA stream, before resampling. At
pitch p its output duration is approximately window/p. It is therefore a different
parameter from the fixed output-grain duration of the previous engine.

## Signal path

RIthm_01.raw is copied from SPI Flash to PSRAM at startup. It must already exist under
that exact name and contain headerless mono signed 16-bit little-endian PCM at
44.1 kHz. Flash is read only. No source recording is included in this repository.
Missing PSRAM/file, invalid byte count, or allocation failure disables playback
and reports the cause to Serial.

WSOLA uses a synthesis hop H equal to half the analysis window. Each next fragment
is selected near the position where the original-speed transport predicts it is
needed. At constant pitch p this gives an analysis hop of approximately H/p. Its
first half is crossfaded with the previous fragment's second half. The resulting
stream is resampled with linear interpolation at rate p.

Candidate fragments are ranked by mean-removed normalized cross-correlation with
the overlap tail. A small distance penalty favors positions near the nominal
transport when matches are similar. The search scans every eight source samples,
then refines near the best position at one-sample resolution. Correlation samples
are decimated for bounded work: approximately 128 samples in the coarse pass and
256 in the fine pass, except for shorter windows. Only one contiguous source
region is copied from PSRAM per search; candidate comparisons use internal RAM.
Silence/constant overlaps favor the nominal position. At pitch 1 the nominal
position is used directly and reproduces the source apart from gain/rounding.

The nominal timeline is anchored to the output transport, which advances by one
source sample per output sample. Selected offsets are local and do not accumulate
into tempo drift. Two buffered hops provide interpolation across hop boundaries.
For varying pitch, the next hop's lookahead uses the current smoothed pitch; this
is a bounded local approximation, not a guarantee of exact transient positioning.

Pitch and gain are smoothed per sample with a 256-sample time constant. Start/stop
uses approximately 5 ms fades. A window change fades out, rebuilds the WSOLA state
at the current transport, and fades in. Changing the search radius takes effect
when another hop is generated. Restart after a completed stop begins at sample zero.

## Serial diagnostics

Output only; no terminal commands are accepted. Every two seconds:

- DIAG: maximum render time, maximum whole-audio CPU use, audio-pool peak and missed allocations.
- LEVEL: original-source RMS and actual output RMS in dBFS, output peak in PCM units,
  latest overlap correlation, latest search offset in source samples, and late blocks.

Output RMS includes LineOutVol. At the default 25 percent volume, a preserved
signal is approximately 12 dB below the original source. Compare different pitch
settings at the same volume. Source and output RMS windows follow the nominal
transport, so local alignment/lookahead can affect comparisons around transients.
Correlation is the most recent overlap estimate, not an average; -1 also identifies
silence or an overlap with negligible variance. A nonzero late count means the
renderer exceeded the approximately 2.9 ms audio-block budget. Search is executed
inside the audio callback: small windows, wide search and high pitch are the most
expensive combination. Hardware timing remains to be established.

Logging runs in loop and drops lines if the terminal cannot accept them immediately.
Controller scanning runs at up to 2 kHz with a 25 ms button debounce.

## Hardware

Teensy 4.1 at 600 MHz; Audio Board Rev D using standard I2S and SGTL5000 control.
Both output channels carry the same mono signal. Line-out level is 13 and headphone
level is 0.8, matching the earlier prototype. Flash uses SPI CS 6. Controls use
SPI1: MOSI 26, SCK 27, MISO 39, CS 37. Display CS 38 stays high.

| Control | MCP23S17 chip | DT | CLK | Active button |
| --- | --- | --- | --- | --- |
| Step | 3 | a3 | a4 | a2 |
| From | 3 | a6 | a7 | none |
| To | 3 | a1 | a0 | none |
| LineOutVol | 4 | a2 | a1 | none |

The pinned Teensyduino 1.62 core supports Lilla's two ISSI 16 MiB PSRAM chips.
No effects, MIDI, display UI, recording or Flash writes are used.

## Build and tests

```powershell
pio run
pio device monitor --baud 115200
g++ -std=c++20 -O2 -Wall -Wextra -Werror -I include src/WsolaCore.cpp tests/wsola_test.cpp -o .pio/wsola_test.exe
./.pio/wsola_test.exe
python tests/encoder_test.py
```

If pio is absent from PATH, use the installed executable at
C:/Users/Utente/.platformio/penv/Scripts/platformio.exe.

Firmware: .pio/build/teensy41/firmware.hex, also exported as Granular_v0_2_4.hex.
No upload is performed during development. Load the HEX with Teensy Loader for
testing; Granular_v0_1_2.hex provides the previous listening baseline.

Host tests verify unity reconstruction from the first samples, window transitions,
source wrapping, continuous pitch changes, transport, stop/restart and pitch/RMS
at 67 Hz and 997 Hz for ratios 0.25, 0.5, 1, 1.3, 2 and 4. These tones are not
chosen to align with the window. At 67 Hz and pitch 0.5, search +/-10 ms restores
RMS to about 2121 versus 1521 with search off (12000 peak input, volume 25 percent).
This demonstrates the alignment effect on a tone, not quality on complex music.

## Listening pass and limitations

At pitch 0.5, keep search at 10 ms and compare windows 10, 15, 20, 25, 30 and 40 ms.
Check LEVEL and DIAG, especially late, then compare other pitch settings. Keep volume fixed.
Actual RIthm_01.raw quality and CPU headroom require physical Lilla measurements.

This implementation has no transient detector, phase-vocoder stage, formant
preservation, or automatic gain compensation. Linear resampling has no dedicated
anti-alias filter for upward transposition; bright material can still alias.
WSOLA cannot align every component of polyphonic or percussive material at once.
The cyclic source wrap has no separate endpoint crossfade. Large pitch changes
can reveal the buffered overlap and local alignment approximation.

## Provenance

Lilla hardware mapping and encoder basis: Sandro Grassia's firmware, source commit
283739101236e5ebba9e0c3b307b56c8177b23e9. The committed granular baseline is b3e1811.
WSOLA is implemented here without importing an external processing library.
Algorithm references: https://www.isca-archive.org/eurospeech_1993/roelands93_eurospeech.html
and https://www.surina.net/article/time-and-pitch-scaling.html.
Teensy Audio, SerialFlash and Adafruit libraries retain their own licenses.

## Processing-time measurements (0.2.4)

Every two seconds BENCH identifies the settings and the audio-block budget:
128 samples at 44.1 kHz, approximately 2902.49 microseconds at 600 MHz.
TIME values use the hardware cycle counter, converted to microseconds.

- engine_us[min/avg/max]: rendering 128 samples, including any WSOLA generation.
- callback_us[avg/max]: allocation, rendering, reference RMS/peak metering and output delivery. Final timing-statistics bookkeeping is excluded.
- WARM: first 260 active blocks after a control change (approximately 755 ms), including startup or parameter transitions.
- READ: steady blocks without a new WSOLA hop.
- GEN: steady blocks generating one or more WSOLA hops.
- ALL: READ and GEN combined, excluding WARM and stopped playback.
- n: measured blocks; hops: generated hops; hmax: maximum hops in one block; late: callbacks exceeding the block budget.

A control change resets the measurement interval and increments its id. Statistics
reset every report; WARM can span reports. AudioProcessorUsageMax also includes
other audio objects and measurement bookkeeping. Serial output can drop lines
if the terminal cannot accept them immediately.

For an initial comparison, keep search at 10 ms and test windows 15 and 20 ms,
with pitch 0.25, 0.5, 1, 2 and 4. Hold each setting for at least 10 seconds and
preferably a complete source loop. Save BENCH, TIME, DIAG and LEVEL lines.
Keep WARM maxima separately and compare steady averages and GEN maxima.
The approximately 2902 us budget is shared by all voices and other audio work;
one-voice measurements alone cannot establish safe Lilla polyphony because
PSRAM/cache contention and coincident generation peaks can change the cost.
Hardware timings must be acquired after loading this firmware; host tests do
not measure Teensy performance.
