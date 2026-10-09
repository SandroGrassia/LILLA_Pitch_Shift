#pragma once
#include <cstdint>

// Call setters with audio IRQ disabled. Source has >= 2 samples; pitch is 0.25..4;
// window is even, 220..4410 samples; search radius is 0..882; volume is 0..1.
class WsolaCore
{
public:
    static constexpr uint32_t MaxWindow = 4410;
    static constexpr uint32_t MaxHop = MaxWindow / 2;
    static constexpr uint32_t MaxSearch = 882;
    void Attach(const int16_t *samples, uint32_t count);
    void SetPitch(float value);
    void SetWindow(uint32_t samples);
    void SetSearch(uint32_t samples);
    void SetVolume(float value);
    void SetRunning(bool value);
    int16_t Render();
    int16_t ReferenceSample() const;
    uint32_t Transport() const;
    float Pitch() const;
    float Correlation() const;
    int32_t Offset() const;

private:
    const int16_t *source = nullptr;
    uint32_t source_count = 0;
    uint32_t transport = 0;
    uint32_t requested_window = 882;
    uint32_t hop = 441;
    uint32_t search_radius = 441;
    float pitch_target = 1.0f;
    float pitch_now = 1.0f;
    float volume_target = 0.25f;
    float volume_now = 0.25f;
    float gate = 0.0f;
    bool running = false;
    bool initialized = false;
    bool geometry_pending = false;
    float read_position = 0.0f;
    uint32_t current = 0;
    int16_t blocks[2][MaxHop] = {};
    int16_t tail[MaxHop] = {};
    int16_t search[MaxWindow + 2 * MaxSearch] = {};
    float last_correlation = 1.0f;
    int32_t last_offset = 0;
    uint32_t Wrap(int64_t position) const;
    void CopySource(int16_t *destination, uint32_t position, uint32_t count) const;
    float Score(uint32_t offset, uint32_t stride) const;
    void Generate(uint32_t destination);
    void Prime();
};
