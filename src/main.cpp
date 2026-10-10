#include <Arduino.h>
#include <Audio.h>
#include <SerialFlash.h>
#include <SPI.h>
#include <smalloc.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "GranularAudio.h"
#include "HardwareControls.h"

namespace
{
GranularAudio player;
AudioOutputI2S output;
AudioConnection left(player, 0, output, 0);
AudioConnection right(player, 0, output, 1);
AudioControlSGTL5000 codec;
HardwareControls controls;
constexpr char filename[] = "Giotto.raw";
constexpr const char *scope_names[] = {"P1", "P2", "MIX"};
int pitch_cents[GranularAudio::VoiceCount] = {};
int volume_percent = 25;
bool running[GranularAudio::VoiceCount] = {};
bool ready = false;
uint32_t last_diagnostics = 0;
uint32_t measurement_id = 0;
char log_buffer[4096];
size_t log_size = 0;
size_t log_sent = 0;

void PrintLine(const char *message)
{
    // Queue the diagnostic burst without blocking the controls or audio.
    const size_t size = strlen(message);
    if (log_size + size + 2u <= sizeof(log_buffer))
    {
        memcpy(log_buffer + log_size, message, size);
        log_size += size;
        log_buffer[log_size++] = '\r';
        log_buffer[log_size++] = '\n';
    }
}

void FlushLogs()
{
    if (Serial && log_sent < log_size)
    {
        const size_t available = static_cast<size_t>(Serial.availableForWrite());
        const size_t remaining = log_size - log_sent;
        const size_t count = available < remaining ? available : remaining;
        if (count > 0)
        {
            log_sent += Serial.write(reinterpret_cast<const uint8_t *>(log_buffer + log_sent), count);
        }
        if (log_sent == log_size)
        {
            log_sent = 0;
            log_size = 0;
        }
    }
}

void ApplyControls()
{
    const float pitches[] = {exp2f(pitch_cents[0] / 1200.0f), exp2f(pitch_cents[1] / 1200.0f)};

    AudioNoInterrupts();
    for (uint32_t voice = 0; voice < GranularAudio::VoiceCount; ++voice)
    {
        player.core[voice].SetPitch(pitches[voice]);
        player.core[voice].SetVolume(volume_percent / 100.0f);
        player.core[voice].SetRunning(running[voice]);
    }
    player.ResetMeasurement();
    AudioProcessorUsageMaxReset();
    AudioMemoryUsageMaxReset();
    AudioInterrupts();

    ++measurement_id;
    last_diagnostics = millis();
    char message[180];
    snprintf(message, sizeof(message), "WSOLA P1=%s pitch=%.4f (%d cents) P2=%s pitch=%.4f (%d cents) window=20 ms search=+/-10 ms overlap=50%% volume=%d%% mix_gain=0.5", running[0] ? "PLAY" : "STOP", pitches[0], pitch_cents[0], running[1] ? "PLAY" : "STOP", pitches[1], pitch_cents[1], volume_percent);
    PrintLine(message);
}

void PrintTiming(uint32_t scope, const char *name, const GranularAudio::Timing &timing)
{
    if (timing.blocks == 0)
    {
        return;
    }
    const double cycles_per_us = F_CPU_ACTUAL / 1000000.0;
    char message[230];
    if (scope == GranularAudio::Mix)
    {
        snprintf(message, sizeof(message), "TIME MIX %s id=%lu n=%lu engine_us[min/avg/max]=%.2f/%.2f/%.2f callback_us[avg/max]=%.2f/%.2f hops=%lu hmax=%lu late=%lu", name, static_cast<unsigned long>(measurement_id), static_cast<unsigned long>(timing.blocks), timing.engine_min / cycles_per_us, timing.engine_cycles / (cycles_per_us * timing.blocks), timing.engine_max / cycles_per_us, timing.callback_cycles / (cycles_per_us * timing.blocks), timing.callback_max / cycles_per_us, static_cast<unsigned long>(timing.hops), static_cast<unsigned long>(timing.hops_max), static_cast<unsigned long>(timing.late));
    }
    else
    {
        snprintf(message, sizeof(message), "TIME %s %s id=%lu n=%lu engine_us[min/avg/max]=%.2f/%.2f/%.2f hops=%lu hmax=%lu", scope_names[scope], name, static_cast<unsigned long>(measurement_id), static_cast<unsigned long>(timing.blocks), timing.engine_min / cycles_per_us, timing.engine_cycles / (cycles_per_us * timing.blocks), timing.engine_max / cycles_per_us, static_cast<unsigned long>(timing.hops), static_cast<unsigned long>(timing.hops_max));
    }
    PrintLine(message);
}

void PrintTimings(uint32_t scope, const GranularAudio::Timings &timings)
{
    PrintTiming(scope, "WARM", timings.transient);
    PrintTiming(scope, "READ", timings.read);
    PrintTiming(scope, "GEN", timings.generate);
    GranularAudio::Timing steady = timings.read;
    steady.blocks += timings.generate.blocks;
    steady.engine_cycles += timings.generate.engine_cycles;
    steady.callback_cycles += timings.generate.callback_cycles;
    steady.engine_min = min(steady.engine_min, timings.generate.engine_min);
    steady.engine_max = max(steady.engine_max, timings.generate.engine_max);
    steady.callback_max = max(steady.callback_max, timings.generate.callback_max);
    steady.hops += timings.generate.hops;
    steady.hops_max = max(steady.hops_max, timings.generate.hops_max);
    steady.late += timings.generate.late;
    PrintTiming(scope, "ALL", steady);
}

float Dbfs(uint64_t energy, uint32_t samples)
{
    const float rms = samples > 0 ? sqrtf(static_cast<float>(energy) / samples) : 0.0f;
    return rms > 0.0f ? 20.0f * log10f(rms / 32768.0f) : -120.0f;
}

bool LoadSource()
{
    char message[120];
    snprintf(message, sizeof(message), "PSRAM pool: %lu KiB", static_cast<unsigned long>(extmem_smalloc_pool.pool_size / 1024u));
    PrintLine(message);
    if (extmem_smalloc_pool.pool_size == 0)
    {
        PrintLine("ERROR: no PSRAM; playback disabled.");
        return false;
    }
    if (!SerialFlash.begin(6))
    {
        PrintLine("ERROR: Flash initialization failed; playback disabled.");
        return false;
    }
    SerialFlashFile file = SerialFlash.open(filename);
    if (!file)
    {
        PrintLine("ERROR: Rithm_01.raw not found in Flash; playback disabled.");
        return false;
    }
    const uint32_t bytes = file.size();
    if (bytes < 4u || (bytes & 1u) != 0u || bytes > extmem_smalloc_pool.pool_size)
    {
        PrintLine("ERROR: invalid PCM size or source larger than PSRAM.");
        file.close();
        return false;
    }
    auto *samples = static_cast<int16_t *>(sm_malloc_pool(&extmem_smalloc_pool, bytes));
    if (samples == nullptr)
    {
        PrintLine("ERROR: PSRAM allocation failed.");
        file.close();
        return false;
    }
    for (uint32_t offset = 0; offset < bytes; offset += 4096u)
    {
        const uint32_t count = bytes - offset < 4096u ? bytes - offset : 4096u;
        if (file.read(reinterpret_cast<uint8_t *>(samples) + offset, count) != count)
        {
            PrintLine("ERROR: incomplete Flash read.");
            sm_free_pool(&extmem_smalloc_pool, samples);
            file.close();
            return false;
        }
    }
    file.close();

    AudioNoInterrupts();
    for (uint32_t voice = 0; voice < GranularAudio::VoiceCount; ++voice)
    {
        player.core[voice].Attach(samples, bytes / sizeof(int16_t));
        player.core[voice].SetWindow(882);
        player.core[voice].SetSearch(441);
    }
    player.SetReady();
    AudioInterrupts();

    snprintf(message, sizeof(message), "Loaded : %lu bytes, %.3f seconds. Flash remains unchanged.", static_cast<unsigned long>(bytes), bytes / (2.0f * AUDIO_SAMPLE_RATE_EXACT));
    PrintLine(message);
    return true;
}
}

void setup()
{
    Serial.begin(115200);
    while (!Serial && millis() < 3000u)
    {
        yield();
    }
    PrintLine("Granular 0.3.0 - two independent WSOLA players - tempo 1");
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
    ready = LoadSource();
    if (ready)
    {
        ApplyControls();
        PrintLine("From: P1 pitch / press=play-stop; To: P2 pitch / press=play-stop; LineOutVol: master volume; Step: unused.");
    }
}

void loop()
{
    FlushLogs();
    if (!ready)
    {
        return;
    }
    controls.Poll();
    bool changed = false;
    controls.TakeRotation(HardwareControls::Step);
    controls.TakeStepPress();
    const int deltas[] = {controls.TakeRotation(HardwareControls::From), controls.TakeRotation(HardwareControls::To)};
    const bool presses[] = {controls.TakeFromPress(), controls.TakeToPress()};
    for (uint32_t voice = 0; voice < GranularAudio::VoiceCount; ++voice)
    {
        const int next_pitch = constrain(pitch_cents[voice] + deltas[voice] * 10, -1200, 1200);
        if (next_pitch != pitch_cents[voice])
        {
            pitch_cents[voice] = next_pitch;
            changed = true;
        }
        if (presses[voice])
        {
            running[voice] = !running[voice];
            changed = true;
        }
    }
    const int next_volume = constrain(volume_percent + controls.TakeRotation(HardwareControls::Volume), 0, 100);
    if (next_volume != volume_percent)
    {
        volume_percent = next_volume;
        changed = true;
    }
    if (changed)
    {
        ApplyControls();
    }
    if (millis() - last_diagnostics >= 2000u)
    {
        last_diagnostics = millis();
        uint32_t maximum;
        uint32_t missed;
        float cpu_max;
        uint16_t pool_max;
        GranularAudio::Levels levels[GranularAudio::VoiceCount + 1];
        GranularAudio::Timings timings[GranularAudio::VoiceCount + 1];
        float correlation[GranularAudio::VoiceCount];
        int32_t offset[GranularAudio::VoiceCount];

        AudioNoInterrupts();
        player.TakeDiagnostics(maximum, missed);
        for (uint32_t scope = 0; scope <= GranularAudio::Mix; ++scope)
        {
            levels[scope] = player.TakeLevels(scope);
            timings[scope] = player.TakeTimings(scope);
        }
        for (uint32_t voice = 0; voice < GranularAudio::VoiceCount; ++voice)
        {
            correlation[voice] = player.core[voice].Correlation();
            offset[voice] = player.core[voice].Offset();
        }
        cpu_max = AudioProcessorUsageMax();
        pool_max = AudioMemoryUsageMax();
        AudioProcessorUsageMaxReset();
        AudioMemoryUsageMaxReset();
        AudioInterrupts();

        char message[230];
        snprintf(message, sizeof(message), "BENCH id=%lu P1_play=%u P1_pitch=%.4f P2_play=%u P2_pitch=%.4f window_ms=20 search_ms=10 volume=%d mix_gain=0.5 block=%u budget_us=%.2f MHz=%.0f", static_cast<unsigned long>(measurement_id), running[0] ? 1u : 0u, exp2f(pitch_cents[0] / 1200.0f), running[1] ? 1u : 0u, exp2f(pitch_cents[1] / 1200.0f), volume_percent, AUDIO_BLOCK_SAMPLES, AUDIO_BLOCK_SAMPLES * 1000000.0 / AUDIO_SAMPLE_RATE_EXACT, F_CPU_ACTUAL / 1000000.0);
        PrintLine(message);
        PrintTimings(GranularAudio::Mix, timings[GranularAudio::Mix]);
        snprintf(message, sizeof(message), "DIAG render_max=%.1f us audio_cpu_max=%.1f%% pool_max=%u missed=%lu late=%lu", maximum / (F_CPU_ACTUAL / 1000000.0f), cpu_max, pool_max, static_cast<unsigned long>(missed), static_cast<unsigned long>(levels[GranularAudio::Mix].over_budget));
        PrintLine(message);
        const GranularAudio::Levels &mix = levels[GranularAudio::Mix];
        snprintf(message, sizeof(message), "LEVEL MIX out=%.1f dBFS peak=%u", Dbfs(mix.output_energy, mix.samples), mix.peak);
        PrintLine(message);
        for (uint32_t voice = 0; voice < GranularAudio::VoiceCount; ++voice)
        {
            PrintTimings(voice, timings[voice]);
            snprintf(message, sizeof(message), "LEVEL %s src=%.1f out=%.1f dBFS peak=%u corr=%.3f offset=%ld (before mix_gain)", scope_names[voice], Dbfs(levels[voice].source_energy, levels[voice].samples), Dbfs(levels[voice].output_energy, levels[voice].samples), levels[voice].peak, correlation[voice], static_cast<long>(offset[voice]));
            PrintLine(message);
        }
    }
}
