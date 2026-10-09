#pragma once
#include <cstdint>

// One mono voice, up to sixteen overlapping grains. Setters run with audio IRQ disabled.
// Caller supplies count >= 2, pitch 0.25..4, length >= 32, grain count 2/4/8/16, slope 0.05..0.5, volume 0..1.
class GranularCore
{
public:
    static constexpr uint32_t GrainCount = 4; // Startup density.
    static constexpr uint32_t MaxGrains = 16;
    void Attach(const int16_t *samples, uint32_t count);
    void SetPitch(float value);
    void SetGrain(uint32_t samples, float slope);
    void SetGrainCount(uint32_t value);
    void SetVolume(float value);
    void SetRunning(bool value);
    int16_t Render();
    uint32_t Transport() const;
    float Pitch() const;

private:
    struct Grain
    {
        uint32_t index = 0;
        float fraction = 0.0f;
        uint32_t age = 0;
        uint32_t length = 2204;
        float ramp = 551.0f;
    };
    Grain grains[MaxGrains];
    uint32_t active_count = GrainCount;
    uint32_t requested_count = GrainCount;
    const int16_t *source = nullptr;
    uint32_t source_count = 0;
    uint32_t transport = 0;
    uint32_t grain_length = 2204;
    float grain_slope = 0.25f;
    float pitch_target = 1.0f;
    float pitch_now = 1.0f;
    float volume_target = 0.25f;
    float volume_now = 0.25f;
    float gate = 0.0f;
    bool running = false;
    bool initialized = false;
    bool geometry_pending = false;
    void Prime();
    void Restart(Grain &grain);
};
