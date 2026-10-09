#pragma once
#include <Arduino.h>
#include <AudioStream.h>
#include "GranularCore.h"

class GranularAudio : public AudioStream
{
public:
    GranularAudio();
    GranularCore core;
    void SetReady();
    void TakeDiagnostics(uint32_t &maximum, uint32_t &missed);
    void update() override;

private:
    bool ready = false;
    uint32_t maximum_cycles = 0;
    uint32_t missed_blocks = 0;
};
