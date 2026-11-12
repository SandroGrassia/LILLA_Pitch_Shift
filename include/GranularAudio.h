#pragma once
#include <Arduino.h>
#include <AudioStream.h>
#include "WsolaCore.h"

class GranularAudio : public AudioStream
{
public:
    explicit GranularAudio(WsolaCore *voices = nullptr);
    static constexpr uint32_t VoiceCount = 12;
    static constexpr uint32_t Mix = VoiceCount;
    WsolaCore *core; // Constructed once in PSRAM before SetReady; no allocation in the audio callback.
    struct Levels
    {
        uint64_t source_energy = 0;
        uint64_t output_energy = 0;
        uint32_t samples = 0;
        uint32_t over_budget = 0;
        uint16_t peak = 0;
    };
    struct Timing
    {
        uint64_t engine_cycles = 0;
        uint64_t callback_cycles = 0;
        uint32_t blocks = 0;
        uint32_t engine_min = UINT32_MAX;
        uint32_t engine_max = 0;
        uint32_t callback_max = 0;
        uint32_t hops = 0;
        uint32_t hops_max = 0;
        uint32_t late = 0;
    };
    struct Timings
    {
        Timing transient;
        Timing read;
        Timing generate;
    };
    Levels TakeLevels(uint32_t scope);
    Timings TakeTimings(uint32_t scope);
    void ResetMeasurement(); // Caller disables the audio IRQ; first 260 active blocks are transient.
    void ResetVoiceMeasurement(uint32_t voice); // Reset only the new note, leaving other voices and MIX untouched.
    void SetReady();
    void TakeDiagnostics(uint32_t &maximum, uint32_t &missed);
    void update() override;

private:
    Levels levels[VoiceCount + 1];
    Timings timings[VoiceCount + 1];
    uint32_t transient_remaining[VoiceCount + 1] = {};
    bool ready = false;
    uint32_t maximum_cycles = 0;
    uint32_t missed_blocks = 0;
    int16_t fallback[AUDIO_BLOCK_SAMPLES];
    int16_t rendered[VoiceCount][AUDIO_BLOCK_SAMPLES];
    int16_t reference[AUDIO_BLOCK_SAMPLES];
    void RecordBlock(uint32_t scope, uint32_t engine, uint32_t callback, uint32_t hops, uint32_t budget);
    void RecordLevel(Levels &target, int32_t original, int32_t output);
    void RecordTiming(Timing &target, uint32_t engine, uint32_t callback, uint32_t hops, uint32_t budget);
};
