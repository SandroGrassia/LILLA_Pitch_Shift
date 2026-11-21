/*
   LILLA Audio Sampler
   Author: Sandro Grassia (info@lillasampler.it)
   www.lillasampler.it
*/

#pragma once
#include <cstdint>
#include "FftSpectralFrame.h"
#include "FftDiagnostics.h"

#ifndef FFT_DUAL_BAND
#define FFT_DUAL_BAND 1
#endif

// Every instance owns its mutable FFT and low-band filtering workspace.
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
    void FinishForwardAt(uint32_t end);
    PlayMode Mode() const { return mode; }
    static constexpr uint32_t Size = 1024;
    static constexpr uint32_t Hop = 256;
    static constexpr uint32_t Bins = Size / 2 + 1;
    using SampleReader = int16_t (*)(void *context, uint32_t position);
    void SetSampleReader(SampleReader reader, void *context);
    static constexpr uint32_t SourceLookahead = 5120; // Long analysis, FIR context, interpolation and scheduling margin.
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
    uint32_t GeneratedHops() const; // High-band cadence; callback timing includes both bands.
    uint32_t AnalyzedFrames() const;
    float Pitch() const;
    uint32_t TransientResets() const;
    void ReadReferenceBlock(int16_t *destination, uint32_t first, uint32_t count) const;
    struct InitialAnalysis
    {
        float phase[2][Bins];
        float magnitude[2][Bins];
        float frequency[2][Bins];
    };
    void SaveInitialAnalysis(InitialAnalysis &data) const;
    void LoadInitialAnalysis(const InitialAnalysis &data);
    bool PrepareStartStep(); // One bounded preparation stage; no transport or envelope advancement.
    int16_t Render();
    FftCoreDiagnostics TakeDiagnostics(); // Caller excludes rendering.


private:
    FftCoreDiagnostics diagnostics;
    SampleReader sample_reader = nullptr;
    void *reader_context = nullptr;
    float ReadSource(uint32_t position) const;
    FftSpectralFrame high;
    FftSpectralFrame low;
    static constexpr uint32_t LowFactor = 4;
    static constexpr uint32_t FilterTaps = 65;
    static bool filter_ready;
    static float decimator[FilterTaps];
    float low_input[Hop * LowFactor + FilterTaps] = {};
    float low_filtered[Size] = {};
    uint32_t low_prepare_position = 0;
    int low_prepare_offset = 0;
    uint8_t low_parts = 0;
    uint8_t start_step = 0;
    void InitializeRender();
    void FilterLowPart(uint32_t position, int offset, uint32_t part);
    void FinishLowAnalysis();
    uint32_t low_until_frame = 0;
    uint32_t low_phase = 0;
    bool low_primed = false;
    bool low_reanchor = false;
    float low_samples[4] = {};
    float ReadOffset(uint32_t position, int offset) const;
    void AnalyzeLow(uint32_t position, int offset);
    float NextLow(int offset);
    float RenderLow();
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
    uint32_t until_frame = 0;
    float pitch_target = 1.0f;
    float pitch_now = 1.0f;
    float volume_target = 0.25f;
    float volume_now = 0.25f;
    float gate = 0.0f;
    bool running = false;
    bool initialized = false;
    float direct_mix = 0.0f;
    uint32_t SourcePosition(uint32_t position) const;
    bool ReadingReverse() const;
    uint32_t Advance(uint32_t position, uint32_t count) const;
    float ReadLoop(uint32_t position) const;
    void Analyze(uint32_t position);
    void Generate();
};
