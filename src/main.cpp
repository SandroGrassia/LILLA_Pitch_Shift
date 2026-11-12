#include <Arduino.h>
#include <Audio.h>
#include <SerialFlash.h>
#include <SPI.h>
#include <smalloc.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include "ExperimentConfig.h"
#include "FileDisplay.h"
#include "RawFiles.h"
#include "GranularAudio.h"
#include "HardwareControls.h"
#include "MidiInput.h"
#include "NoteVoices.h"

namespace
{
FftPitchCore fft_voices[GranularAudio::VoiceCount];
GranularAudio player(fft_voices);
AudioOutputI2S output;
AudioConnection left(player, 0, output, 0);
AudioConnection right(player, 0, output, 1);
AudioControlSGTL5000 codec;
HardwareControls controls;
FileDisplay display;
RawFiles raw_files;
int selected_file = -1;
int loaded_file = -1;
int requested_file = -1;
int loading_file = -1;
uint32_t selection_time = 0;
SerialFlashFile pending_file;
int16_t *pending_samples = nullptr;
uint32_t pending_bytes = 0;
uint32_t pending_read = 0;
MidiInput midi_input;
NoteVoices voices(player);
NoteVoices::Source source;
int volume_percent = 25;
bool ready = false;
uint32_t last_diagnostics = 0;
char log_buffer[16384];
size_t log_read = 0;
size_t log_write = 0;
size_t log_count = 0;

void PrintLine(const char *message)
{
    const size_t size = strlen(message);
    if (size + 2u > sizeof(log_buffer) - log_count)
    {
        return;
    }
    for (size_t i = 0; i < size; ++i)
    {
        log_buffer[log_write] = message[i];
        log_write = (log_write + 1u) % sizeof(log_buffer);
    }
    log_buffer[log_write] = '\r';
    log_write = (log_write + 1u) % sizeof(log_buffer);
    log_buffer[log_write] = '\n';
    log_write = (log_write + 1u) % sizeof(log_buffer);
    log_count += size + 2u;
}

void FlushLogs()
{
    if (Serial && log_count > 0)
    {
        const size_t available = static_cast<size_t>(Serial.availableForWrite());
        const size_t count = std::min(available, std::min(log_count, sizeof(log_buffer) - log_read));
        if (count > 0)
        {
            const size_t sent = Serial.write(reinterpret_cast<const uint8_t *>(log_buffer + log_read), count);
            log_read = (log_read + sent) % sizeof(log_buffer);
            log_count -= sent;
        }
    }
}

void PrintOverlap()
{
    PrintLine("FFT overlap=75% (fixed)");
}

void PrintGrain()
{
    PrintLine("FFT samples=1024 hop=256 (fixed)");
}

GranularAudio::Timing TotalTiming(const GranularAudio::Timings &timings)
{
    GranularAudio::Timing total;
    const GranularAudio::Timing *parts[] = {&timings.transient, &timings.read, &timings.generate};
    for (const auto *part : parts)
    {
        total.blocks += part->blocks;
        total.engine_cycles += part->engine_cycles;
        total.callback_cycles += part->callback_cycles;
        total.engine_min = std::min(total.engine_min, part->engine_min);
        total.engine_max = std::max(total.engine_max, part->engine_max);
        total.callback_max = std::max(total.callback_max, part->callback_max);
        total.hops += part->hops;
        total.hops_max = std::max(total.hops_max, part->hops_max);
        total.late += part->late;
    }
    return total;
}

void ReleaseNote(uint8_t key)
{
    NoteVoices::Report report;

    AudioNoInterrupts();
    voices.NoteOff(key, report);
    AudioInterrupts();
}

void ReleaseAll()
{
    for (uint32_t voice = 0; voice < GranularAudio::VoiceCount; ++voice)
    {
        NoteVoices::Report report;

        AudioNoInterrupts();
        voices.ReleaseVoice(voice, report);
        AudioInterrupts();
    }
    voices.ClearRejected();
}

void HandleMidi(const MidiInput::Message &message)
{
    if (message.type == 0x80 || (message.type == 0x90 && message.data2 == 0))
    {
        ReleaseNote(message.data1);
    }
    else if (message.type == 0x90)
    {
        if (loading_file >= 0)
        {
            return; // During replacement, release tails finish but new notes wait for the next key press.
        }
        uint32_t voice;

        AudioNoInterrupts();
        const auto result = voices.NoteOn(message.data1, message.data2, voice);
        AudioInterrupts();

        char text[200];
        if (result == NoteVoices::Result::Started)
        {
            snprintf(text, sizeof(text), "NOTE_ON pitch=%.5f", voices.TargetPitch(message.data1));
        }
        else
        {
            const char *reason = result == NoteVoices::Result::Full ? "capacity (including anticlick tails)" : result == NoteVoices::Result::OutOfRange ? "outside notes 36..84" : "waiting for rejected NoteOff";
            snprintf(text, sizeof(text), "NOTE_REJECT note=%u reason=%s", message.data1, reason);
        }
        PrintLine(text);
    }
    else if (message.type == 0xE0)
    {
        const int bend = (static_cast<int>(message.data2) << 7) + message.data1 - 8192;

        AudioNoInterrupts();
        voices.PitchBend(bend);
        AudioInterrupts();
    }
    else if (message.type == 0xB0 && (message.data1 == 120 || message.data1 == 123))
    {
        ReleaseAll();
    }
}

bool LoadSource()
{
    char message[180];
    snprintf(message, sizeof(message), "PSRAM pool=%lu KiB; FFT state=%lu bytes/voice in internal RAM", static_cast<unsigned long>(extmem_smalloc_pool.pool_size / 1024u), static_cast<unsigned long>(sizeof(FftPitchCore)));
    PrintLine(message);
    if (extmem_smalloc_pool.pool_size == 0 || !SerialFlash.begin(6))
    {
        PrintLine("ERROR: PSRAM absent or Flash initialization failed; playback disabled.");
        return false;
    }
    raw_files.Scan();
    if (raw_files.Count() == 0)
    {
        display.Show("No RAW files", "None", "Playback disabled");
        PrintLine("ERROR: no valid RAW files in Flash.");
        return false;
    }
    selected_file = raw_files.Find(ExperimentConfig::SourceFile);
    if (selected_file < 0)
    {
        selected_file = 0;
    }
    if (raw_files.Truncated())
    {
        PrintLine("WARNING: RAW catalog limited to 1024 names shorter than 64 bytes.");
    }
    display.Show(raw_files.Name(selected_file), "None", "Loading...");
    SerialFlashFile file = SerialFlash.open(raw_files.Name(selected_file));
    if (!file)
    {
        snprintf(message, sizeof(message), "ERROR: %s not found in Flash; playback disabled.", raw_files.Name(selected_file));
        PrintLine(message);
        return false;
    }
    const uint32_t bytes = file.size();
    if (bytes < 4u || (bytes & 1u) != 0u || bytes > extmem_smalloc_pool.pool_size)
    {
        file.close();
        PrintLine("ERROR: invalid PCM size or source larger than PSRAM.");
        return false;
    }
    auto *samples = static_cast<int16_t *>(sm_malloc_pool(&extmem_smalloc_pool, bytes));
    if (samples == nullptr)
    {
        file.close();
        PrintLine("ERROR: insufficient PSRAM for source.");
        return false;
    }
    for (uint32_t offset = 0; offset < bytes; offset += 4096u)
    {
        const uint32_t count = std::min<uint32_t>(bytes - offset, 4096u);
        if (file.read(reinterpret_cast<uint8_t *>(samples) + offset, count) != count)
        {
            file.close();
            sm_free_pool(&extmem_smalloc_pool, samples);
            PrintLine("ERROR: incomplete Flash read; playback disabled.");
            return false;
        }
    }
    file.close();
    source = {samples, bytes / sizeof(int16_t)};
    voices.SetSource(source);
    loaded_file = selected_file;
    display.Show(raw_files.Name(selected_file), raw_files.Name(loaded_file), "Ready");
    snprintf(message, sizeof(message), "SOURCE file=%s bytes=%lu duration=%.3f s (shared by all 4 voices)", raw_files.Name(selected_file), static_cast<unsigned long>(bytes), bytes / (2.0 * AUDIO_SAMPLE_RATE_EXACT));
    PrintLine(message);
    return true;
}

void ShowFileStatus(const char *status)
{
    display.Show(raw_files.Name(selected_file), raw_files.Name(loaded_file), status);
}

void FailFileLoad(const char *reason)
{
    pending_file.close();
    if (pending_samples != nullptr)
    {
        sm_free_pool(&extmem_smalloc_pool, pending_samples);
        pending_samples = nullptr;
    }
    loading_file = -1;
    PrintLine(reason);
    ShowFileStatus(reason);
}

void PollFileLoad()
{
    if (loading_file < 0)
    {
        if (requested_file < 0 || millis() - selection_time < 250u)
        {
            return;
        }
        const int target = requested_file;
        requested_file = -1;
        if (target == loaded_file)
        {
            ShowFileStatus("Ready");
            return;
        }
        pending_file = SerialFlash.open(raw_files.Name(target));
        if (!pending_file)
        {
            FailFileLoad("File unavailable");
            return;
        }
        pending_bytes = pending_file.size();
        pending_samples = static_cast<int16_t *>(sm_malloc_pool(&extmem_smalloc_pool, pending_bytes));
        if (pending_samples == nullptr)
        {
            FailFileLoad("Not enough free PSRAM");
            return;
        }
        loading_file = target;
        pending_read = 0;
        ReleaseAll();
        ShowFileStatus("Loading - notes stopped");
    }
    if (pending_read < pending_bytes)
    {
        const uint32_t count = std::min<uint32_t>(4096u, pending_bytes - pending_read);
        if (pending_file.read(reinterpret_cast<uint8_t *>(pending_samples) + pending_read, count) != count)
        {
            FailFileLoad("File read failed");
            return;
        }
        pending_read += count;
        return; // Keep servicing MIDI, encoders and diagnostics between chunks.
    }
    bool active = false;

    AudioNoInterrupts();
    for (uint32_t voice = 0; voice < GranularAudio::VoiceCount; ++voice)
    {
        active = active || player.core[voice].IsActive();
    }
    AudioInterrupts();

    if (active)
    {
        return; // Old source must survive until every anticlick tail is finished.
    }
    const int16_t *old_samples = source.samples;
    source = {pending_samples, pending_bytes / sizeof(int16_t)};

    AudioNoInterrupts();
    voices.SetSource(source);
    voices.ClearRejected();
    for (uint32_t voice = 0; voice < GranularAudio::VoiceCount; ++voice)
    {
        player.core[voice].Attach(source.samples, source.count);
    }
    player.ResetMeasurement();
    AudioInterrupts();

    pending_samples = nullptr;
    pending_file.close();
    sm_free_pool(&extmem_smalloc_pool, const_cast<int16_t *>(old_samples));
    loaded_file = loading_file;
    loading_file = -1;
    ShowFileStatus(requested_file >= 0 ? "Selection pending" : "Ready");
    char message[100];
    snprintf(message, sizeof(message), "FILE loaded=%s", raw_files.Name(loaded_file));
    PrintLine(message);
}

void PollControls()
{
    controls.Poll();
    const int selection = controls.TakeRotation(HardwareControls::Select);
    if (selection != 0)
    {
        selected_file = constrain(selected_file + selection, 0, raw_files.Count() - 1);
        requested_file = selected_file;
        selection_time = millis();
        ShowFileStatus(loading_file >= 0 ? "Loading..." : "Selection pending");
    }
    const int overlap_rotation = controls.TakeRotation(HardwareControls::Step);
    const int grain_rotation = controls.TakeRotation(HardwareControls::To);
    const int next_volume = constrain(volume_percent + controls.TakeRotation(HardwareControls::Volume), 0, 100);
    controls.TakeRotation(HardwareControls::From);
    controls.TakeStepPress();
    controls.TakeFromPress();
    controls.TakeToPress();
    if (next_volume != volume_percent)
    {
        volume_percent = next_volume;

        AudioNoInterrupts();
        voices.SetVolume(volume_percent);
        AudioInterrupts();
    }
    if (overlap_rotation != 0)
    {
        PrintOverlap();
    }
    if (grain_rotation != 0)
    {
        PrintGrain();
    }
}

void PrintDiagnostics()
{
    uint32_t active = 0;
    GranularAudio::Timings timings;

    AudioNoInterrupts();
    timings = player.TakeTimings(GranularAudio::Mix);
    for (uint32_t voice = 0; voice < GranularAudio::VoiceCount; ++voice)
    {
        active += player.core[voice].IsActive() ? 1u : 0u;
    }
    AudioInterrupts();

    const auto total = TotalTiming(timings);
    const double cycles_per_us = F_CPU_ACTUAL / 1000000.0;
    const double average = total.blocks > 0 ? total.engine_cycles / (cycles_per_us * total.blocks) : 0.0;
    const double maximum = total.engine_max / cycles_per_us;
    const double budget = AUDIO_BLOCK_SAMPLES * 1000000.0 / AUDIO_SAMPLE_RATE_EXACT;
    char message[180];
    snprintf(message, sizeof(message), "PLAYERS n=%lu cost_us[avg/max]=%.2f/%.2f cost_pct[avg/max]=%.1f/%.1f", static_cast<unsigned long>(active), average, maximum, average * 100.0 / budget, maximum * 100.0 / budget);
    PrintLine(message);
}
}

void setup()
{
    Serial.begin(115200);
    while (!Serial && millis() < 3000u)
    {
        yield();
    }
    PrintLine("Granular 0.6.3 - FFT phase vocoder - 4 voices - MIDI DIN CH1 - LOOP_FWD tempo 1");
    AudioMemory(12);
    if (!controls.Begin())
    {
        PrintLine("ERROR: MCP23S17 initialization failed.");
        return;
    }
    display.Begin();
    display.Show(ExperimentConfig::SourceFile, "None", "Starting...");
    if (!codec.enable())
    {
        PrintLine("ERROR: Audio Board initialization failed.");
        return;
    }
    codec.lineOutLevel(13);
    codec.volume(0.8f);
    codec.audioProcessorDisable();
    if (!LoadSource())
    {
        display.Show(selected_file >= 0 ? raw_files.Name(selected_file) : ExperimentConfig::SourceFile, "None", "Load failed - see serial");
        return;
    }
    midi_input.Begin();

    AudioNoInterrupts();
    player.ResetMeasurement();
    player.SetReady();
    AudioInterrupts();

    ready = true;
    PrintLine("FFT 1024 samples; overlap 75%; hop 256; To/Step rotation: show fixed value; Select: RAW file; LineOutVol: master 0..100%; buttons unused.");
    PrintLine("MIDI notes 36..84; velocity gain; bend +/-2st clamped to pitch 0.25..4; no ADSR, existing 5ms anticlick only. CC120/123: release all.");
    PrintOverlap();
    PrintGrain();
}

void loop()
{
    FlushLogs();
    if (!ready)
    {
        return;
    }
    const uint8_t count = midi_input.Collect();
    for (uint8_t i = 0; i < count; ++i)
    {
        HandleMidi(midi_input.messages[i]);
    }
    PollControls();
    PollFileLoad();
    if (millis() - last_diagnostics >= 2000u)
    {
        last_diagnostics = millis();
        PrintDiagnostics();
    }
}
