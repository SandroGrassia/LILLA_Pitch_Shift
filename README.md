# Granular 0.4.0 - WSOLA experiment

Standalone PlatformIO firmware for the existing Lilla hardware. The active engine
uses waveform-similarity overlap-add (WSOLA) followed by resampling, replacing the
uniformly scheduled granular engine from version 0.1.2. The previous GranularCore
and its tests remain in the repository as a baseline; they are not connected to audio.

## Controls

| Encoder | Rotation | Pushbutton |
| --- | --- | --- |
| Step | Common overlap: 10, 25, 50, 75 percent | Unused |
| From | Player 1 pitch 0.5 to 2, in 10-cent steps | Player 1 play/stop |
| To | Player 2 pitch 0.5 to 2, in 10-cent steps | Player 2 play/stop |
| LineOutVol | Master volume: 0 to 100 percent | Unused |

Both players start stopped, with pitch 1 and master volume 25 percent.
Window is fixed at 20 ms (882 samples), search at +/-10 ms (441 samples).
Overlap starts at 50 percent. Step selects 10, 25, 50 or 75 percent for both voices.
Changes fade out, rebuild at the current transport and fade back in; they do not restart the source.
The original complementary-ramp path is retained at 50 percent. Other settings use
normalized weighted overlap-add; 75 percent retains contributions from up to four grains.
Each voice has its own transport, pitch smoothing, start/stop fade and WSOLA state.
Both share a single read-only source allocation in PSRAM. Controls for one voice
never restart or reposition the other voice. Restart after a completed stop begins
at sample zero; a press during the stop fade reverses the fade without restarting.

The mono mix is (player1 + player2) / 2, duplicated to left and right outputs.
This fixed headroom prevents sum overflow even for coincident full-scale voices.
A solo voice is therefore 6 dB quieter than in 0.2.4 at the same volume setting;
the factor does not change when the other voice starts or stops.

Window duration refers to the intermediate WSOLA stream, before resampling. At
pitch p its output duration is approximately window/p. It is therefore a different
parameter from the fixed output-grain duration of the previous engine.

## Signal path

Giotto.raw is copied from SPI Flash to PSRAM at startup. It must already exist under
that exact name and contain headerless mono signed 16-bit little-endian PCM at
44.1 kHz. Flash is read only. No source recording is included in this repository.
Missing PSRAM/file, invalid byte count, or allocation failure disables playback
and reports the cause to Serial.

WSOLA uses a synthesis hop H equal to the window minus the integer overlap length. Each next fragment
is selected near the position where the original-speed transport predicts it is
needed. At constant pitch p this gives an analysis hop of approximately H/p. Its
leading overlap is aligned with the pending mixed tail and combined using linear ramps. The resulting
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
uses approximately 5 ms fades. A window or overlap change fades out, rebuilds the WSOLA state
at the current transport, and fades in. Changing the search radius takes effect
when another hop is generated. Restart after a completed stop begins at sample zero.

## Serial diagnostics

Output only; no terminal commands are accepted. Every two seconds:

- BENCH: both players' play/pitch settings, window/search geometry, selected overlap, volume and block budget.
- TIME MIX: measured combined rendering time and complete audio callback time.
- TIME P1 / P2: rendering time for each active voice, before reference metering.
- DIAG: maximum callback time, maximum whole-audio CPU use, audio-pool peak, missed allocations and callbacks over budget (late).
- LEVEL MIX: actual mixed output RMS in dBFS and PCM peak.
- LEVEL P1 / P2: source/output RMS, peak, latest correlation and offset, before the fixed mix gain.

Voice output levels include master volume but precede the half-gain mix. At 25 percent
volume, a preserved voice is approximately 12 dB below its source before mixing,
and approximately 18 dB below when heard alone after mixing. Reference metering
follows the nominal transport; it is approximate around startup and transients.
Correlation is the latest overlap estimate, not an average; -1 also identifies
silence or an overlap with negligible variance.

Logs use a nonblocking 4096-byte queue, drained as USB accepts data. Lines are
dropped if the queue is full. No terminal input is used. Controls are scanned at
up to 2 kHz with a 25 ms button debounce.

## Hardware

Teensy 4.1 at 600 MHz; Audio Board Rev D using standard I2S and SGTL5000 control.
Both output channels carry the same mono signal. Line-out level is 13 and headphone
level is 0.8, matching the earlier prototype. Flash uses SPI CS 6. Controls use
SPI1: MOSI 26, SCK 27, MISO 39, CS 37. Display CS 38 stays high.

| Control | MCP23S17 chip | DT | CLK | Active button |
| --- | --- | --- | --- | --- |
| Step | 3 | a3 | a4 | unused (a2) |
| From | 3 | a6 | a7 | a5 |
| To | 3 | a1 | a0 | b6 |
| LineOutVol | 4 | a2 | a1 | none |

The pinned Teensyduino 1.62 core detects the installed PSRAM automatically, including
two 8 MiB chips (16 MiB total) or two ISSI 16 MiB chips. The source must fit the
available pool and is allocated once for both voices. FRAM is not used.
No effects, MIDI, display UI, recording or Flash writes are used.

## Build and tests

```powershell
pio run
pio device monitor --baud 115200
g++ -std=c++20 -O2 -Wall -Wextra -Werror -I include src/WsolaCore.cpp tests/wsola_test.cpp -o .pio/wsola_test.exe
./.pio/wsola_test.exe
g++ -std=c++20 -O2 -Wall -Wextra -Werror -I include src/WsolaCore.cpp tests/overlap_test.cpp -o .pio/overlap_test.exe
./.pio/overlap_test.exe
python tests/encoder_test.py
g++ -std=c++20 -O2 -Wall -Wextra -Werror -I tests/stubs -I include src/WsolaCore.cpp src/GranularAudio.cpp tests/two_voice_test.cpp -o .pio/two_voice_test.exe
./.pio/two_voice_test.exe
```

If pio is absent from PATH, use the installed executable at
C:/Users/Utente/.platformio/penv/Scripts/platformio.exe.

Firmware: .pio/build/teensy41/firmware.hex, also exported as Granular_v0_4_0.hex.
No upload is performed during development. Load the HEX with Teensy Loader for
testing; Granular_v0_1_2.hex provides the previous listening baseline.

Host tests verify unity reconstruction from the first samples, window transitions,
source wrapping, continuous pitch changes, transport, stop/restart and pitch/RMS
at 67 Hz and 997 Hz for ratios 0.25, 0.5, 1, 1.3, 2 and 4. These tones are not
chosen to align with the window. At 67 Hz and pitch 0.5, search +/-10 ms restores
RMS to about 2121 versus 1521 with search off (12000 peak input, volume 25 percent).
This demonstrates the alignment effect on a tone, not quality on complex music.
The two-voice test exercises the actual audio wrapper with a mocked AudioStream:
sample-exact mixing against independent cores, coincident hop counts, independent
stop/pitch, statistics reset, allocation failure and full-scale headroom. Its clock
is mocked; it cannot validate physical timing or encoder wiring.

## Listening pass and limitations

First test From and To independently, then both players at pitch 2 for at least
10 seconds, preferably a full source loop. Repeat with different start times,
both buttons pressed together, and continuous independent pitch changes.
Check TIME MIX maxima, audio_cpu_max, late and missed. Starting both together is
useful but does not guarantee sample-synchronous starts because of button scanning
and debounce; MIX hmax=2 confirms two generations occurred within one callback.
Actual two-voice CPU headroom still requires physical Teensy measurements.

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

## Processing-time measurements (0.3.0)

The shared callback budget is 128 samples at 44.1 kHz, approximately 2902.49 us.
Hardware cycle-counter readings are converted to microseconds at the actual CPU clock.

- engine_us[min/avg/max]: rendering 128 samples; MIX sums both measured render intervals in that same callback, including an idle renderer if present.
- callback_us[avg/max]: MIX only; actual elapsed time from entry through allocation, both renderers, reference metering, mixing, output delivery and per-voice timing statistics. Final MIX timing bookkeeping is excluded.
- WARM: first 260 active blocks after a control change, tracked separately per voice and for MIX (approximately 755 ms).
- READ: steady blocks with no new WSOLA hop in that scope.
- GEN: steady blocks with at least one new hop; MIX includes either or both voices generating.
- ALL: READ and GEN combined; excludes WARM and fully stopped scopes.
- n: measured blocks; hops: total generated hops; hmax: most hops in one block. MIX hmax=2 identifies coincident work from both voices at the permitted steady pitches.
- late: MIX callbacks exceeding the shared budget. DIAG also includes idle callbacks.

Any effective control change resets all timing/level intervals, audio CPU maxima,
and the measurement id. It does not reset transports or force re-priming of an
already playing voice. Each two-second report clears accumulated statistics;
remaining WARM counts persist. Individual and MIX ALL groups can cover different
blocks if a voice starts or stops, so their averages should not be added blindly.
The measured MIX callback is the reference for total cost.

Single-voice 0.2.4 hardware measurements at this geometry gave approximately
165 us mean / 1019 us steady maximum at pitch 0.5, and 573 us mean / 982 us maximum
at pitch 2 (callback times). They motivate this dual-voice experiment, but cannot
establish safe polyphony inside the full Lilla firmware. Simultaneous searches,
PSRAM/cache behavior and other audio work must be measured together.

## Overlap experiment (0.4.0)

At window 882, the selected overlap lengths are 88, 220, 441 and 661 samples;
the synthesis hops are 794, 662, 441 and 221 samples respectively. Percentages
are rounded down to whole samples. Pitch remains 0.5..2 from From/To. Step rotation
increases/decreases overlap without wrapping; its pushbutton is unused.
Serial remains output-only and reports the overlap in WSOLA and BENCH lines.

At 75 percent several tails are accumulated and normalized sample by sample.
Correlation uses the normalized pending tail. At 10/25 percent the middle of
each grain is unblended. Window/search remain fixed at 20 ms and +/-10 ms.
The 50-percent audio path is preserved for comparison; larger buffers increase
RAM use for all settings. Earlier hardware timings describe only the old 50-percent
configuration and must not be applied to the new overlap settings.

Compare both voices at pitch 2 for each overlap; watch TIME MIX maxima, late and
missed, including WARM after changes. At 75 percent two generations per voice
can fall in one callback. Check transients, low notes, level stability and encoder
changes during playback. Host tests cannot establish hardware CPU headroom or sound quality.
