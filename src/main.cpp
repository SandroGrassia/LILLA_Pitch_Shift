#include <Arduino.h>
#include <Audio.h>
#include <SerialFlash.h>
#include <SPI.h>
#include <smalloc.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <new>
#include "ExperimentConfig.h"
#include "GranularAudio.h"
#include "HardwareControls.h"
#include "MidiInput.h"
#include "NoteVoices.h"

namespace
{
GranularAudio player;
AudioOutputI2S output;
AudioConnection left(player, 0, output, 0);
AudioConnection right(player, 0, output, 1);
AudioControlSGTL5000 codec;
HardwareControls controls;
MidiInput midi_input;
NoteVoices voices(player);
NoteVoices::Source sources[2];
constexpr uint32_t overlaps[] = {10, 25, 50, 75};
constexpr int overlap_id = 2;
constexpr int grain_ms = 20;
int volume_percent = 25;
bool antialias_enabled = true;
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

uint32_t WindowSamples()
{
    // Nearest even source-sample count; WSOLA requires an even window, 220..4410.
    return static_cast<uint32_t>(grain_ms * (44100.0f / 2000.0f) + 0.5f) * 2u;
}

void PrintOverlap()
{
    char message[48];
    snprintf(message, sizeof(message), "OVERLAP=%lu%% (fixed)", static_cast<unsigned long>(overlaps[overlap_id]));
    PrintLine(message);
}

void PrintGrain()
{
    char message[48];
    snprintf(message, sizeof(message), "GRAIN samples=%lu (fixed)", static_cast<unsigned long>(WindowSamples()));
    PrintLine(message);
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

bool LoadSources()
{
    char message[180];
    snprintf(message, sizeof(message), "PSRAM pool=%lu KiB; WSOLA state=%lu bytes/voice x %lu", static_cast<unsigned long>(extmem_smalloc_pool.pool_size / 1024u), static_cast<unsigned long>(sizeof(WsolaCore)), static_cast<unsigned long>(GranularAudio::VoiceCount));
    PrintLine(message);
    if (extmem_smalloc_pool.pool_size == 0 || !SerialFlash.begin(6))
    {
        PrintLine("ERROR: PSRAM absent or Flash initialization failed; playback disabled.");
        return false;
    }
    void *storage = sm_malloc_pool(&extmem_smalloc_pool, sizeof(WsolaCore) * GranularAudio::VoiceCount);
    if (storage == nullptr)
    {
        PrintLine("ERROR: cannot allocate WSOLA voices in PSRAM.");
        return false;
    }
    player.core = new (storage) WsolaCore[GranularAudio::VoiceCount];
    for (uint32_t source = 0; source < 2; ++source)
    {
        if (source == 1 && strcmp(ExperimentConfig::SourceFiles[0], ExperimentConfig::SourceFiles[1]) == 0)
        {
            sources[1] = sources[0];
            PrintLine("SOURCE B shares SOURCE A (same filename; no duplicate allocation).");
            continue;
        }
        SerialFlashFile file = SerialFlash.open(ExperimentConfig::SourceFiles[source]);
        if (!file)
        {
            snprintf(message, sizeof(message), "ERROR: %s not found in Flash; playback disabled.", ExperimentConfig::SourceFiles[source]);
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
            PrintLine("ERROR: insufficient free PSRAM for audio sources and voice states.");
            return false;
        }
        sources[source] = {samples, bytes / sizeof(int16_t)};
        for (uint32_t offset = 0; offset < bytes; offset += 4096u)
        {
            const uint32_t count = std::min<uint32_t>(bytes - offset, 4096u);
            if (file.read(reinterpret_cast<uint8_t *>(samples) + offset, count) != count)
            {
                file.close();
                PrintLine("ERROR: incomplete Flash read; playback disabled.");
                return false;
            }
        }
        file.close();
        snprintf(message, sizeof(message), "SOURCE %c file=%s bytes=%lu duration=%.3f s", 'A' + static_cast<int>(source), ExperimentConfig::SourceFiles[source], static_cast<unsigned long>(bytes), bytes / (2.0 * AUDIO_SAMPLE_RATE_EXACT));
        PrintLine(message);
    }
    voices.SetSources(sources[0], sources[1]);
    return true;
}

void FreeSources()
{
    if (sources[1].samples != nullptr && sources[1].samples != sources[0].samples)
    {
        sm_free_pool(&extmem_smalloc_pool, const_cast<int16_t *>(sources[1].samples));
    }
    if (sources[0].samples != nullptr)
    {
        sm_free_pool(&extmem_smalloc_pool, const_cast<int16_t *>(sources[0].samples));
    }
    if (player.core != nullptr)
    {
        for (uint32_t voice = 0; voice < GranularAudio::VoiceCount; ++voice)
        {
            player.core[voice].~WsolaCore();
        }
        sm_free_pool(&extmem_smalloc_pool, player.core);
        player.core = nullptr;
    }
}

void PollControls()
{
    controls.Poll();
    const int overlap_rotation = controls.TakeRotation(HardwareControls::Step);
    const int grain_rotation = controls.TakeRotation(HardwareControls::To);
    const int next_volume = constrain(volume_percent + controls.TakeRotation(HardwareControls::Volume), 0, 100);
    controls.TakeRotation(HardwareControls::From);
    const bool toggle_antialias = controls.TakeStepPress();
    controls.TakeFromPress();
    controls.TakeToPress();
    if (next_volume != volume_percent || toggle_antialias)
    {
        volume_percent = next_volume;
        if (toggle_antialias)
        {
            antialias_enabled = !antialias_enabled;
        }

        AudioNoInterrupts();
        voices.SetParameters(WindowSamples(), overlaps[overlap_id], volume_percent);
        voices.SetAntialias(antialias_enabled);
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
    if (toggle_antialias)
    {
        PrintLine(antialias_enabled ? "AA=ON" : "AA=BYPASS");
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
    PrintLine("Granular 0.5.6 - MIDI DIN Serial1 CH1 - WSOLA 12 voices - per-candidate reference 64/128 - LOOP_FWD tempo 1");
    AudioMemory(12);
    if (!controls.Begin())
    {
        PrintLine("ERROR: MCP23S17 initialization failed.");
        return;
    }
    if (!codec.enable())
    {
        PrintLine("ERROR: Audio Board initialization failed.");
        return;
    }
    codec.lineOutLevel(13);
    codec.volume(0.8f);
    codec.audioProcessorDisable();
    if (!LoadSources())
    {
        FreeSources();
        return;
    }
    midi_input.Begin();

    AudioNoInterrupts();
    player.ResetMeasurement();
    player.SetReady();
    AudioInterrupts();

    ready = true;
    PrintLine("Grain fixed at 882 samples; overlap fixed at 50%; To/Step rotation: show fixed value; Step press: AA bypass; LineOutVol: master 0..100%.");
    PrintLine("MIDI notes 36..84; velocity gain; bend +/-2st clamped to pitch 0.25..4; no ADSR, existing 5ms anticlick only. CC120/123: release all.");
    PrintOverlap();
    PrintGrain();
    PrintLine(antialias_enabled ? "AA=ON" : "AA=BYPASS");
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
    if (millis() - last_diagnostics >= 2000u)
    {
        last_diagnostics = millis();
        PrintDiagnostics();
    }
}
