#pragma once
#include <cstdint>

// Serialized rendering across all instances: FFT workspace is shared in internal RAM.
// Source has at least 2 samples, pitch is 0.25..4, gain is 0..1. Setters require audio IRQ exclusion.
class FftPitchCore
{
public:
    static constexpr uint32_t Size = 1024;
    static constexpr uint32_t Hop = 256;
    static constexpr uint32_t Bins = Size / 2 + 1;
    FftPitchCore();
    void Attach(const int16_t *samples, uint32_t count);
    void StartNote(const int16_t *samples, uint32_t count, float pitch, float volume);
    void SetPitch(float value);
    void SetVolume(float value);
    void SetRunning(bool value);
    bool IsActive() const;
    uint32_t Transport() const;
    uint32_t GeneratedHops() const;
    uint32_t AnalyzedFrames() const;
    float Pitch() const;
    uint32_t TransientResets() const;
    void ReadReferenceBlock(int16_t *destination, uint32_t first, uint32_t count) const;
    int16_t Render();

private:
    const int16_t *source = nullptr;
    uint32_t source_count = 0;
    uint32_t transport = 0;
    uint32_t head = 0;
    uint32_t until_frame = 0;
    uint32_t generated = 0;
    uint32_t analyzed = 0;
    uint32_t transient_resets = 0;
    uint32_t transient_holdoff = 0;
    float pitch_target = 1.0f;
    float pitch_now = 1.0f;
    float volume_target = 0.25f;
    float volume_now = 0.25f;
    float gate = 0.0f;
    bool running = false;
    bool initialized = false;
    bool first_frame = true;
    bool analysis_ready = false;
    bool frame_transient = false;
    float direct_mix = 0.0f;
    float analysis_frequency[Bins] = {};
    float previous_phase[Bins] = {};
    float previous_magnitude[Bins] = {};
    float synthesis_phase[Bins] = {};
    float accumulation[Size] = {};
    static bool tables_ready;
    static float window[Size];
    alignas(32) static float fft[2 * Size];
    static float magnitude[Bins];
    static float frequency_sum[Bins];
    static float strongest[Bins];
    static float initial_phase[Bins];
    static float advanced_phase[Bins];
    static uint16_t dominant_source[Bins];
    static uint16_t peaks[Bins];
    static uint16_t peak_owner[Bins];
    static uint16_t phase_anchor[Bins];
    static bool advance_needed[Bins];
    static void Transform(bool inverse);
    static float WrapPhase(float phase);
    void Analyze(uint32_t position);
    void Generate();
};
