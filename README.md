# LILLA_PitchShift v0_0_9 - R2 integration

The default PlatformIO target is now `lilla_fft_r2`. `lilla/` contains the complete
WHOLE_LILLA application imported from main commit `0eba4b3`, with its AudioPlayer
using the shared `include/FftPitchCore.h` / `src/FftPitchCore.cpp` engine.
The main branch is unchanged. The previous standalone experiment remains available
with `platformio run -e teensy41`; its controls and firmware documentation are below.

## Staged dual-band work (v0_0_9)

The low-band FIR is now prepared in four 256-sample decimated chunks (each reads
1024 source samples plus FIR context), spaced by 128 output samples. The low FFT
analysis is performed on a later callback. Filtered samples are kept per voice;
transform and short read scratch remain shared and serialized. Synthesis keeps
using the pitch at the original output instant. Mid-frame control invalidation
can still require synchronous recovery; those edit-time peaks need hardware checks.

AudioPlayer prepares four initial FIR chunks, then low and high analyses over six
callbacks before rendering the attack. Transport and ADSR do not advance while
waiting. The two player identities start on opposite callback parities. This adds
about 17.4..20.3 ms of NoteOn startup latency, including at unity pitch, in exchange
for distributing startup work. The existing Live source margin remains 5120 samples;
the preparation wait adds to end-to-end latency. No extra delay is added at steady state.

Timing reports retain two voices. start_max_us now includes preparation callbacks
as well as the first rendered block; starts counts only first rendered blocks.
The pipeline was checked against the previous PCM on 94/95/105 at pitch 2 and
prepared/immediate attack equivalence is tested across all modes and multiple pitches.
R2 listening confirmed that clicks disappeared with two voices. Observed maximum
CPU was 77.3%, start/preparation maximum 1234 us, steady player maximum 999 us,
and deadline stops remained zero in the reported run. These measurements do not
certify every source/effect configuration.

## Transition timing probe (v0_0_8)

Two-player DSP and scheduling are unchanged. With USB Serial connected, FFT_TIMING
reports every two seconds when sufficient serial buffer space is available.
CPU maximum covers the audio update; start_max_us and run_max_us are per-player
render maxima (including effects, excluding final delivery and cleanup). starts
counts first rendered blocks; deadline_stops counts emergency voice stops since
the previous report. active is an instantaneous count, not an interval maximum.
A CPU peak over 100% or a deadline stop supports an overload diagnosis; neither
proves the cause of every audible interruption. No I2S underrun counters are added.

## Two-player trial (v0_0_7)

Limit physical AudioPlayer and ADSR arrays to two entries and remove routing
connections for slots 2 and 3. The dual-band FFT, pitch limits, effects and Live
margin are unchanged. This isolates polyphony cost when investigating clicks
during note transitions. Hardware confirmation is pending.

## Dual-band engine update (v0_0_6)

Ported the accepted R1 baseline from tag `baseline-r1-0.7.11` (commit `1857d39`),
not from main, which still contains the original WHOLE_LILLA application.
The high band retains FFT 1024/hop 256; the low band uses a 65-tap anti-alias FIR,
decimation by four and FFT 1024/hop 256, covering 4096 source samples. Peak regions
are blended over 200..400 Hz of the input spectrum. Cubic interpolation returns
the low band to the original sample rate. No rejected offline variants are included.

The callback source reader is preserved for Flash, PSRAM and live capture, including
zero-filled unrecorded samples. Recording-stop drain invalidates both prepared
analyses. Live startup now needs 5120 recorded samples (~116 ms); this is not a
measured end-to-end latency. Shared transform and decimator scratch live in RAM2
to retain RAM1 stack headroom; per-voice phase/overlap histories remain in RAM1.

Two AudioPlayers and first-instrument-only playback are retained. The increased
CPU and source-read cost must be measured on R2; two slots do not guarantee two
real-time voices with every effect/storage configuration. Source admission now
reserves coincident short/long analyses, so Flash playback may be rejected earlier.
The R1 listening improvement on low bass is established, but the full Lilla port
still needs listening and timing checks on hardware. The wall/crumbling artifact
remains an unresolved limitation of the accepted baseline.

## Integrated behavior

- Two physical player slots, shared by Performance, MIDI Loop, preview and Live.
- Base transposition -24..+12 semitones relative to each instrument root key.
  NoteOn outside that range is rejected before allocating a voice. Combined note
  and preset tuning is clamped to 0.25..2 before the additional +/-2-semitone bend;
  total pitch is bounded to 0.22272468..2.24492410, including vibrato.
- Lilla's minimum region length remains 100 samples. All six play modes use FFT.
- ADSR, velocity, gain ramps, pan, TONE, VCF/LFO, resolution/downsampling and routing
  remain in Lilla. Unity pitch uses the RAW output path with FFT history kept warm.
- Stored sources use direct PSRAM access or a two-bank 256-sample Flash reader,
  including packetized recordings. FFT handles region bounds, direction and XMIX.
- Live reads the existing mono/stereo circular PSRAM buffers through the same FFT
  engine. Unrecorded first-fill samples return zero and unavailable loops wait.
  Mobile forward starts maintain at least 5120 recorded samples of history
  (116.10 ms at 44.1 kHz); locked regions wait without changing their endpoints.
  ADSR attack waits for the source to become ready. Released waiting notes can end.
- Live ONCE_FWD follows recording at fixed original tempo, irrespective of pitch.
  At recording stop it drains the recorded remainder and ends. Resuming recording
  fades the old block and reanchors the forward stream with the FFT margin.
  Both forward and reverse loops, and both ping-pong modes, share the same reader.
- First FFT frames are prepared over six callbacks without advancing source or
  ADSR. The two voices start on opposite callback parities; startup adds about
  17.4..20.3 ms before the attack. Steady-state low filtering is split into four jobs.
- Replacement notes fade the outgoing block before reusing its FFT state. Region
  edits retain the prototype's fade/reset behavior; ONCE direction and ping-pong
  startup-direction edits retain their existing continuity behavior.

## Build and verification

Build: `platformio run -e lilla_fft_r2` (also the default target).
The build derives its HEX/ELF name from `FIRMWARE_VERSION` in `lilla/lib/config/config.h`.
The current HEX is `LILLA_PitchShift_v0_0_9.hex`. It is exported to the
repository root and checked byte-for-byte against the corresponding named HEX in
`.pio/build/lilla_fft_r2/`. Teensy GUI upload explicitly selects this root copy,
so Loader displays the release name. The copy is refreshed and verified before upload.
No automatic upload is performed.

Run `python tests/region_controls_test.py` for the shared DSP regression suite.
Run `python tests/lilla_fft_adapter_test.py` for the actual AudioPlayer source and
Live adapter methods, compiled against a host hardware shim. These tests compare
Flash/PSRAM rendering with the standalone engine and simulate ongoing capture,
ring overwrite, startup waiting, first-fill protection and recording-stop drain.
They do not replace an R2 test of the full application, MIDI/ADSR/effects scheduling,
physical storage, display, or real input-to-output latency.

The 116 ms Live margin is a source-read requirement, not a measured end-to-end
latency; input/output buffering adds time. Two voices are the maximum; available headroom depends on effects and source storage. Source-transfer
admission and Lilla's cycle deadline remain enabled; FFT CPU plus the complete
R2 effects chain must be measured on hardware. The old speed-shift harvesting and interpolation functions have been removed
from AudioPlayer; update uses only the FFT rendering path. Display drawing code
is linked into Flash while shared temporary FFT buffers use RAM2 and voice histories use RAM1.

---

# Standalone prototype reference

# LILLA Pitch Shift 0.7.10 - FFT regions and loop cross-mix

Branch `fft-region-controls`, based on `develop`. This is the R1 prototype
(16 MiB PSRAM, MIDI DIN channel 1), not yet the full Lilla integration.
The engine retains five voices, FFT 1024/hop 256, phase-region mapping and the
unity-pitch RAW bypass from 0.6.8. Playback supports LOOP_FWD, ONCE_FWD, ONCE_REV, LOOP_REV, LOOP_FWD_REV and LOOP_REV_FWD; startup remains LOOP_FWD.

## Operating limits

Maximum polyphony is five voices, including anticlick tails. The mixer divides
by five, so a single voice is approximately 1.58 dB louder than with the previous
six-voice normalization at the same volume setting.
MIDI notes 36..72 cover -24..+12 semitones around root note 60. Pitch bend adds
+/-2 semitones across the entire range, including the endpoints: the total pitch
ratio is approximately 0.222725..2.244924 (-26..+14 semitones).
The selectable region minimum remains 100 samples, matching WHOLE_LILLA's
MIN_SNIPPET. Files shorter than 100 samples retain their full-file minimum.
The DSP's two-sample host tests do not expose a smaller UI trim range.

## Active controls

- From rotation: adjust the first sample, inclusive.
- To rotation: adjust the last sample, inclusive.
- Step rotation: select 1, 10, 100, 1000, 10000 samples or current region length/16.
  The default is length/16, recalculated as the region changes, with a floor of 1.
- Step press: restore the length/16 step.
- From press: reset the first sample to zero.
- To press: toggle TO (independent endpoints) / SLICE (From moves both endpoints
  while preserving length). To still adjusts the end/length in either mode.
- R1 physical Value rotation (Lilla logical ID 11): load another RAW file, resetting bounds to the whole file
  and returning to TO mode, with cross-mix off. The chosen step mode is retained.
- R1 physical Select rotation (Lilla logical ID 8): adjust cross-mix length independently of Step.
  Zero disables it. Lilla limits apply: 300 samples maximum, or 42 samples when
  the region length is <=674 (Lilla wavetable threshold), also capped to half
  the region for very short files. Shrinking clamps it; expanding does not restore it.
  Each detent adds 2 below 30, 4 from 30 to below 90, or 10 from 90 upward.
  Decreasing subtracts 2 at <=30, 4 at <=90, or 10 above 90, clamped to valid bounds.
- Press R1 physical Select: choose XMIX or PLAY_MODE editing (shown on display).
  Rotation follows ONCE_FWD <-> ONCE_REV <-> LOOP_FWD <-> LOOP_FWD_REV <->
  LOOP_REV_FWD <-> LOOP_REV, clamped at either end, one adjacent transition per
  controls poll. This follows Lilla Performance/Sound edit ordering; no wrap or long press.
- LineOutVol: master volume as before.

Trim semantics and the 100-sample minimum follow Lilla main's Sound editor.
Files shorter than 100 samples use their full length as the minimum.
There is no patch-cache length limit: this prototype uses the loaded PSRAM file.
Step is applied per collected rotation batch. Bounds never cross or leave the file.
Display and REGION serial messages show both inclusive endpoints, length, step,
and TO/SLICE state; the display also shows duration and a region bar.
Controls are ignored during active file loading; loading failure retains the old
source and bounds. Edits apply globally to all active and subsequent notes.

## Live-edit behavior

An edit queues the latest valid region. Each active voice fades the old region
out over approximately 5 ms, discards analyzed/overlap FFT history, and fades the
new region in over 1024 output samples (23.22 ms), allowing overlap to build.
The absolute source position is preserved if still in bounds when the change
is applied; otherwise it jumps to From. New notes always start at From.
Rapid edits coalesce to the latest request; NoteOff still ends the voice.
No allocation or logging occurs in the callback; the IRQ-excluded publication
only updates metadata, leaving resets and fades to the renderer.

This is deliberately a fade-out/restart/fade-in, not a seamless dual-engine
crossfade. Old-region audio can remain during the initial 5 ms fade. A short
volume dip is expected; fast repeated edits can prolong attenuation. With cross-mix off, loop seams retain the existing 0.6.8 behavior. Simultaneous five-voice edits reset multiple FFTs in one callback:
measure peak callback cost on R1 before treating this as real-time verified.
Both ping-pong modes are available; all six play modes can be selected.

## Loop cross-mix

The source reader linearly blends the last N samples of the selected region
with its first N samples. Weights sum to one; this is not an equal-power mix.
Both FFT analysis and the unity-pitch direct reader use that same virtual source,
so no untransposed signal is mixed into the pitched output. The original PSRAM
samples are never modified; no extra FFT voice or sample buffer is allocated.

The final mixed sample matches source From+N-1. At the loop boundary reading
continues at From+N, avoiding a second playback of the head already blended.
Thus the first traversal has the full selected length; subsequent loop periods
are region length minus N output samples, independent of MIDI pitch. No-click
behavior is not guaranteed for every signal: opposite-phase content can cancel,
and the FFT retains its ordinary reconstruction artifacts.

Cross-mix-only changes apply at the next synthesis frame (at most 256 output
samples, about 5.8 ms). Transport, phase history, overlap buffers and gain remain
continuous: there is no fade-out or restart. A frame analyzed ahead using the old
seam is reanalyzed once, so measure callback peaks while turning Select. In ONCE
the setting is stored without altering playback. Bounds and loop-mode changes retain the
existing fade/reset/fade-in transition. Rapid mix requests coalesce to the latest
value. This removes the forced playback interruption; seam edits may still alter
the waveform and must be assessed for clicks on hardware.
The R1 labels are intentionally mapped by function in HardwareControls:
FileSelect = physical Value, chip 3 b5/b4; CrossMix = physical Select, chip 3 b2/b1.
Step only selects the From/To increment; it never changes cross-mix or FFT overlap. XMIX on the display
shows samples and milliseconds, while serial also reports effective loop length.
Cross-mix starts disabled to preserve the baseline until explicitly adjusted.

## ONCE_FWD

Press physical Select on R1 (chip 3 b0, logical Lilla button ID 8) to choose
PLAY_MODE editing, then rotate through ONCE_FWD <-> ONCE_REV <-> LOOP_FWD.
The display and serial identify the current mode. Press again to edit XMIX.
ONCE_FWD <-> ONCE_REV preserves the current transport, gate and FFT overlap.
The next analysis reanchors phase/frequency for the new direction; existing overlap
audio drains naturally (up to 1024 samples), without forced fade or retrigger.
At unity pitch the direct reader reverses immediately. Reversing away from the
end fade restores gain over at most 221 samples. Loop transitions and pending
region edits still use fade/reset/fade-in, preserving position when possible. An already finished voice is never resurrected by an edit.

ONCE_FWD reads From through To once at fixed original duration, regardless of pitch.
FFT lookahead is zero-padded beyond To; it never wraps. Loop cross-mix is ignored
but its setting is retained for returning to LOOP_FWD. The final min(221, length)
samples fade to zero, and the voice ends exactly at To without emitting an FFT
tail afterward. This can soften very short snippets; hardware listening is needed.
Early NoteOff still triggers the existing anticlick release.

Finished voices are reclaimed on MIDI events even while their keys remain held.
A fixed 128-entry ledger preserves outstanding NoteOff ownership in NoteOn order,
including repeated keys whose voices finished in a different order. If the ledger
is full, ended notes retain their voice slots until NoteOff instead of losing
ownership. CC120/123 clears pending ownership with the existing all-notes-off path.
No dynamic allocation is introduced. The host suite checks exact duration at
multiple pitches, tiny regions, silent lookahead, ignored XMIX, live mode changes,
capacity after natural end, and late/reordered completion ownership.

## ONCE_REV

ONCE_REV starts each new note at the inclusive To sample and advances backward
one source sample per output sample, independent of pitch. Analysis windows read
backward too, with silence before From. It uses the same final anticlick fade as
ONCE_FWD, ending exactly after the selected region length, and ignores loop XMIX.
The loaded PSRAM buffer is unchanged; no reversed copy is allocated.

Live ONCE direction changes preserve the absolute source position and FFT overlap
without restarting; transitions involving LOOP_FWD retain fade/reset/fade-in. If a region edit excludes the current
position in reverse mode, reading resumes from the new To. A completed reverse
core exposes UINT32_MAX as its transport sentinel; reference metering handles
that as silence. Late NoteOff ownership and voice reclamation are shared with
ONCE_FWD. Tests compare reverse output sample-for-sample to ONCE_FWD over an
independently reversed source, including short regions, boundaries at index zero,
several pitches, live edits, early release and five-voice reuse.

## LOOP_REV

Select PLAY_MODE with the R1 Select button, then rotate through LOOP_FWD,
LOOP_FWD_REV and LOOP_REV_FWD to reach LOOP_REV. The adjacent transitions now
work with active voices; there is no direct LOOP_FWD/LOOP_REV shortcut.

New notes start at To and read backward through From. At the lower seam, the
last N samples blend with the first N samples of the reverse traversal near To.
The next traversal resumes at To-N, avoiding replay of the blended head. Like
this prototype's LOOP_FWD, the first traversal spans the full region; subsequent
periods contain length-N samples. XMIX=0 wraps directly from From to To.
FFT analysis and unity RAW bypass use the same reverse stream. XMIX edits do
not restart the voice; bounds edits retain the existing fade/reset/fade-in.
The engine allocates no reversed source copy or extra FFT voice.
Host tests compare every output sample with LOOP_FWD over an independently
reversed source, including live XMIX edits, multiple pitches, tiny regions,
release, live bounds and five simultaneous voices. Hardware listening/timing
still needs verification.

## LOOP_FWD_REV and LOOP_REV_FWD

Both modes read a ping-pong stream without cross-mix. LOOP_FWD_REV starts a new
note at From moving forward; LOOP_REV_FWD starts at To moving backward. Endpoints
are not duplicated: for a four-sample region the forward-start stream is
A B C D C B A B C D. A full period contains 2*(length-1) output samples, independent
of pitch. FFT windows and lookahead follow this same stream through every turn;
there is no phase/overlap reset, fade or reallocation at the endpoints.

XMIX remains stored for the one-way loop modes but is ignored here, including
live edits, without extra FFT analysis. Display and serial mark it ignored.
Changing between the two ping-pong modes preserves a running voice's current leg,
transport and FFT state; only new notes change their starting direction. This
matches Lilla's shared internal ping-pong trajectory. Transitions involving the
one-way loops and edits of the region bounds retain the short fade/reset/fade-in;
the real source position is preserved if still in bounds, along with the current
leg when entering or editing ping-pong. These transitions are not yet seamless.

The internal transport is a virtual cursor: its descending leg mirrors beyond To.
The source reader and reference meter translate it to an in-bounds sample address.
No reversed sample copy or extra per-voice FFT buffer is allocated. Host tests
compare both modes sample-for-sample against independently constructed ping-pong
streams at pitches 0.5, 1, 1.7 and 3, including two-sample regions, endpoint turns,
ignored mix updates, live mode/bounds edits, meters, releases and five voices.

## Build, validation and firmware

Build with `C:/Users/Utente/.platformio/penv/Scripts/platformio.exe run`.
The active export is `LILLA_Pitch_Shift_v0_7_10.hex`, copied from the freshly built
`.pio/build/teensy41/firmware.hex` and verified by SHA-256. No upload is automatic.
For upload select this exact file, not a historical HEX by date/version alone.

Run `python tests/region_controls_test.py`. Tests cover trim bounds and buttons,
region/cropped-source sample equivalence at multiple pitches, loops as short as
two samples in the DSP, queued edits, release during an edit, five-voice publication,
MIDI allocation, and FFT regressions. Cross-mix tests check independent linear
blend/transport models, tiny regions, exact FFT equivalence to a materialized
virtual stream, seam reduction, live changes and five-voice publication. Full-file rendering is sample-identical to
committed 0.6.8 on the host, including pitch bend and release. Host tests do not
validate display appearance, encoder feel, audible fades or Teensy CPU peaks.

First hardware check: one sustained note at pitch 1 and then 1.5; adjust From/To,
try TO/SLICE and both fine/automatic Step, release during an edit, and load another
file. Then repeat with multiple voices and inspect callback maximum percentage.
The original `Granular_v0_6_8.hex` is retained. The historical firmware-identity
check below intentionally does not apply to this new 0.7.10 firmware.

---

The following documentation describes the preserved 0.6.8 baseline. Its old control
mapping and HEX filename are historical, superseded above on this branch.

# Historical reference: Granular 0.6.8

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
