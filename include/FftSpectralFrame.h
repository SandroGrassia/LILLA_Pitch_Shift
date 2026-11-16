#pragma once
#include <cstdint>

// Serialized callers share the transform workspace. Each instance owns phase and overlap history.
class FftSpectralFrame
{
    friend class FftPitchCore;
public:
    static constexpr uint32_t Size = 1024;
    static constexpr uint32_t Hop = 256;
    static constexpr uint32_t Bins = Size / 2 + 1;
    enum class Band { Full, High, Low };
    FftSpectralFrame();
    void Reset();
    void Input(uint32_t index, float value);
    void Analyze(bool reanchor);
    void Generate(float pitch_now, Band band);
    float Take();
private:
    uint32_t head = 0;
    uint32_t generated = 0;
    uint32_t analyzed = 0;
    uint32_t transient_resets = 0;
    uint32_t transient_holdoff = 0;
    bool first_frame = true;
    bool analysis_ready = false;
    bool frame_transient = false;
    float analysis_frequency[Bins] = {};
    float previous_phase[Bins] = {};
    float previous_magnitude[Bins] = {};
    float synthesis_phase[Bins] = {};
    float accumulation[Size] = {};
    static bool tables_ready;
    static float window[Size];
    alignas(32) static float fft[2 * Size];
    static float advanced_phase[Bins];
    static uint16_t peaks[Bins];
    static uint16_t peak_owner[Bins];
    static void Transform(bool inverse);
    static float WrapPhase(float phase);
    static float LowWeight(float frequency);
};
