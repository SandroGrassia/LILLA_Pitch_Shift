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
    Levels TakeLevels();
    void SetReady();
    void TakeDiagnostics(uint32_t &maximum, uint32_t &missed);
    void update() override;

private:
    Levels levels;
    bool ready = false;
    uint32_t maximum_cycles = 0;
    uint32_t missed_blocks = 0;
};
