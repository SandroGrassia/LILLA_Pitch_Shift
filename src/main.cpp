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
constexpr uint32_t durations_ms[] = {5, 20, 50, 100};
constexpr char filename[] = "Adv_01.raw";
int pitch_cents = 0;
int duration_id = 2;
int slope_percent = 25;
int volume_percent = 25;
bool running = false;
bool ready = false;
uint32_t last_diagnostics = 0;

void PrintLine(const char *message)
{
    // Drop a log line if the terminal cannot accept it; controls and audio stay responsive.
    const size_t size = strlen(message);
    if (Serial && Serial.availableForWrite() >= static_cast<int>(size + 2u))
    {
        Serial.print(message);
        Serial.print("\r\n");
    }
}

void ApplyControls()
{
    const float pitch = exp2f(pitch_cents / 1200.0f);
    const uint32_t length = static_cast<uint32_t>(AUDIO_SAMPLE_RATE_EXACT * durations_ms[duration_id] / 1000.0f) / GranularCore::GrainCount * GranularCore::GrainCount;

    AudioNoInterrupts();
    player.core.SetPitch(pitch);
    player.core.SetGrain(length, slope_percent / 100.0f);
    player.core.SetVolume(volume_percent / 100.0f);
    player.core.SetRunning(running);
    AudioInterrupts();

    char message[160];
    snprintf(message, sizeof(message), "%s pitch=%.4f (%d cents) grain=%lu ms slope=%d%%/edge volume=%d%% grains=%lu", running ? "PLAY" : "STOP", pitch, pitch_cents, static_cast<unsigned long>(durations_ms[duration_id]), slope_percent, volume_percent, static_cast<unsigned long>(GranularCore::GrainCount));
    PrintLine(message);
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
        PrintLine("ERROR: Adv_01.raw not found in Flash; playback disabled.");
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
    player.core.Attach(samples, bytes / sizeof(int16_t));
    player.SetReady();
    AudioInterrupts();

    snprintf(message, sizeof(message), "Loaded Adv_01.raw: %lu bytes, %.3f seconds. Flash remains unchanged.", static_cast<unsigned long>(bytes), bytes / (2.0f * AUDIO_SAMPLE_RATE_EXACT));
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
    PrintLine("Granular 0.1.0 - Lilla hardware - four grains - tempo 1");
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
        PrintLine("Step: pitch / press=start-stop; From: grain; To: slope; LineOutVol: volume.");
    }
}

void loop()
{
    if (!ready)
    {
        return;
    }
    controls.Poll();
    bool changed = false;
    const int pitch_delta = controls.TakeRotation(HardwareControls::Step);
    const int duration_delta = controls.TakeRotation(HardwareControls::From);
    const int slope_delta = controls.TakeRotation(HardwareControls::To);
    const int volume_delta = controls.TakeRotation(HardwareControls::Volume);
    if (pitch_delta != 0 || duration_delta != 0 || slope_delta != 0 || volume_delta != 0)
    {
        pitch_cents = constrain(pitch_cents + pitch_delta * 10, -2400, 2400);
        duration_id = constrain(duration_id + duration_delta, 0, 3);
        slope_percent = constrain(slope_percent + slope_delta, 5, 50);
        volume_percent = constrain(volume_percent + volume_delta, 0, 100);
        changed = true;
    }
    if (controls.TakeStepPress())
    {
        running = !running;
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

        AudioNoInterrupts();
        player.TakeDiagnostics(maximum, missed);
        cpu_max = AudioProcessorUsageMax();
        pool_max = AudioMemoryUsageMax();
        AudioProcessorUsageMaxReset();
        AudioMemoryUsageMaxReset();
        AudioInterrupts();

        char message[128];
        snprintf(message, sizeof(message), "DIAG render_max=%.1f us audio_cpu_max=%.1f%% pool_max=%u missed=%lu", maximum / (F_CPU_ACTUAL / 1000000.0f), cpu_max, pool_max, static_cast<unsigned long>(missed));
        PrintLine(message);
    }
}
