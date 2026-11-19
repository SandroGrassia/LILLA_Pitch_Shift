#pragma once
#include <cstdint>
#include <memory>

// Each band owns its transform workspace, phase history and overlap accumulation.
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
    // Allocated once at construction, using the Teensy RAM2 heap. Extra space
    // preserves 32-byte alignment independently of the allocator alignment.
    std::unique_ptr<float[]> fft_storage;
    float *fft = nullptr;
    float advanced_phase[Bins] = {};
    uint16_t peaks[Bins] = {};
    uint16_t peak_owner[Bins] = {};
    void Transform(bool inverse);
    static float WrapPhase(float phase);
    static float LowWeight(float frequency);
};
