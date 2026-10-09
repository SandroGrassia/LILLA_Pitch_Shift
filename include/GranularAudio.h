#pragma once
#include <Arduino.h>
#include <AudioStream.h>
#include "WsolaCore.h"

class GranularAudio : public AudioStream
{
public:
    GranularAudio();
    WsolaCore core;
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
    Levels TakeLevels();
    Timings TakeTimings();
    void ResetMeasurement(); // Caller disables the audio IRQ; first 260 active blocks are transient.
    void SetReady();
    void TakeDiagnostics(uint32_t &maximum, uint32_t &missed);
    void update() override;

private:
    Levels levels;
    Timings timings;
    uint32_t transient_remaining = 260;
    bool ready = false;
    uint32_t maximum_cycles = 0;
    uint32_t missed_blocks = 0;
    int16_t fallback[AUDIO_BLOCK_SAMPLES];
    int16_t reference[AUDIO_BLOCK_SAMPLES];
    void RecordTiming(Timing &target, uint32_t engine, uint32_t callback, uint32_t hops, uint32_t budget);
};
