#pragma once
#include <cstdint>

// Serialized rendering across all instances: FFT workspace is shared in internal RAM.
// Source has at least 2 samples, pitch is 2^(-26/12)..2^(14/12), gain is 0..1. Setters require audio IRQ exclusion.
class FftPitchCore
{
public:
    enum class PlayMode { LoopForward, OnceForward, OnceReverse, LoopReverse, LoopForwardReverse, LoopReverseForward };
    static bool IsLoop(PlayMode value) { return UsesCrossMix(value) || IsPingPong(value); }
    static bool IsPingPong(PlayMode value) { return value == PlayMode::LoopForwardReverse || value == PlayMode::LoopReverseForward; }
    static bool UsesCrossMix(PlayMode value) { return value == PlayMode::LoopForward || value == PlayMode::LoopReverse; }
    static bool IsReverse(PlayMode value) { return value == PlayMode::OnceReverse || value == PlayMode::LoopReverse || value == PlayMode::LoopReverseForward; }
    void SetMode(PlayMode value);
    void FinishForwardAt(uint32_t end); // Live forward stream: stop at an already recorded end, preserving overlap and transport.
    PlayMode Mode() const { return mode; }
    static constexpr uint32_t Size = 1024;
    static constexpr uint32_t Hop = 256;
    static constexpr uint32_t Bins = Size / 2 + 1;
    using SampleReader = int16_t (*)(void *context, uint32_t position);
    void SetSampleReader(SampleReader reader, void *context);
    FftPitchCore();
    void Attach(const int16_t *samples, uint32_t count);
    void StartNote(const int16_t *samples, uint32_t count, float pitch, float volume);
    void SetRegion(uint32_t first, uint32_t end, uint32_t cross_mix = 0); // End exclusive; valid range within attached source, at least 2 samples.
    uint32_t CrossMix() const { return cross_mix; }
    uint32_t RegionFrom() const { return region_first; }
    uint32_t RegionEnd() const { return region_end; }
    void SetPitch(float value);
    void SetVolume(float value);
    void SetRunning(bool value);
    bool IsActive() const;
    uint32_t Transport() const; // Absolute/virtual cursor: ping-pong descending leg mirrors beyond To; completed ONCE_REV is UINT32_MAX.
    uint32_t GeneratedHops() const;
    uint32_t AnalyzedFrames() const;
    float Pitch() const;
    uint32_t TransientResets() const;
    void ReadReferenceBlock(int16_t *destination, uint32_t first, uint32_t count) const;
    int16_t Render();

private:
    SampleReader sample_reader = nullptr;
    void *reader_context = nullptr;
    float ReadSource(uint32_t position) const;
    const int16_t *source = nullptr;
    uint32_t source_count = 0;
    uint32_t region_first = 0;
    uint32_t region_end = 0;
    uint32_t cross_mix = 0; // 0 or 2..half region length, validated by caller.
    uint32_t requested_mix = 0;
    float cross_mix_scale = 0.0f;
    uint32_t requested_first = 0;
    uint32_t requested_end = 0;
    PlayMode mode = PlayMode::LoopForward;
    PlayMode requested_mode = PlayMode::LoopForward;
    bool region_pending = false;
    float region_gain = 1.0f;
    float end_gain_now = 1.0f;
    bool direction_reanchor = false;
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
    static float advanced_phase[Bins];
    static uint16_t peaks[Bins];
    static uint16_t peak_owner[Bins];
    static void Transform(bool inverse);
    static float WrapPhase(float phase);
    uint32_t SourcePosition(uint32_t position) const;
    bool ReadingReverse() const;
    uint32_t Advance(uint32_t position, uint32_t count) const;
    float ReadLoop(uint32_t position) const;
    void Analyze(uint32_t position);
    void Generate();
};
