#pragma once
#include <cstdint>
#include "WsolaAntialias.h"

// Call setters with audio IRQ disabled. Source has >= 2 samples; pitch is 0.25..4;
// window is even, 220..4410 samples; search radius is 0..882; volume is 0..1.
// Render calls across ALL instances must be serialized: Generate uses shared internal-RAM scratch.
class WsolaCore
{
public:
    static constexpr uint32_t MaxWindow = 4410;
    static constexpr uint32_t MaxHop = MaxWindow * 9 / 10;
    static constexpr uint32_t MaxSearch = 882;
    void Attach(const int16_t *samples, uint32_t count);
    void StartNote(const int16_t *samples, uint32_t count, float pitch, float volume); // Only on an inactive voice; restart at source zero with the existing anticlick fade.
    void SetPitch(float value);
    void SetAntialias(bool enabled); // Smooth live bypass; enabled by default, effective only above unity pitch.
    void SetWindow(uint32_t samples);
    void SetOverlap(uint32_t percent); // Select 10, 25, 50 or 75 percent overlap; changes use the geometry fade.
    void SetSearch(uint32_t samples);
    void SetVolume(float value);
    void SetRunning(bool value);
    int16_t Render();
    int16_t ReferenceSample() const;
    bool IsActive() const;
    uint32_t GeneratedHops() const;
    void ReadReferenceBlock(int16_t *destination, uint32_t first, uint32_t count) const;
    uint32_t Transport() const;
    float Pitch() const;
    float Correlation() const;
    int32_t Offset() const;

private:
    const int16_t *source = nullptr;
    uint32_t source_count = 0;
    uint32_t transport = 0;
    uint32_t requested_window = 882;
    uint32_t requested_overlap = 50; // Requested overlap percentage.
    uint32_t window = 882; // Active source fragment length.
    uint32_t overlap = 441; // Active correlation and crossfade length.
    uint32_t hop = 441;
    uint32_t search_radius = 441;
    float pitch_target = 1.0f;
    float pitch_now = 1.0f;
    WsolaAntialias antialias;
    bool antialias_enabled = true;
    bool antialias_running = false;
    float antialias_mix = 0.0f;
    float filtered_previous = 0.0f;
    float filtered_next = 0.0f;
    int32_t filtered_index = -1;
    uint32_t filter_update = 0;
    float volume_target = 0.25f;
    float volume_now = 0.25f;
    float gate = 0.0f;
    bool running = false;
    bool initialized = false;
    bool geometry_pending = false;
    float read_position = 0.0f;
    uint32_t current = 0;
    int16_t blocks[2][MaxHop] = {};
    int16_t tail[MaxWindow] = {};
    alignas(32) static int16_t search[MaxWindow + 2 * MaxSearch]; // Shared scratch, default .bss places it in Teensy DTCM, not PSRAM.
    alignas(32) static int16_t correlation_tail[MaxWindow]; // Refilled on every Generate; no persistent voice state here.
    float accumulated[MaxWindow] = {}; // Pending weighted samples for non-50-percent overlap.
    float weights[MaxWindow] = {}; // Pending normalization weights.
    uint32_t generated_hops = 0;
    float last_correlation = 1.0f;
    int32_t last_offset = 0;
    uint32_t Wrap(int64_t position) const;
    void CopySource(int16_t *destination, uint32_t position, uint32_t count) const;
    float Score(uint32_t offset, uint32_t stride) const;
    void Generate(uint32_t destination);
    void Prime();
    void AdvanceFilter(int32_t index); // Consume even the samples skipped by upward resampling.
    void MixFragment(uint32_t destination, uint32_t best); // Normalize and emit one hop, retaining all overlapping tails.
};
