#pragma once
#include "FftPitchCore.h"

// Access only from serialized audio callbacks, or with the audio IRQ excluded.
// Sources are immutable PSRAM recordings; callers invalidate before storage reuse.
class FftAttackCache
{
public:
    struct Request
    {
        const int16_t *samples;
        uint32_t count;
        uint32_t source_first;
        uint32_t from;
        uint32_t end;
        uint32_t mix;
        FftPitchCore::PlayMode mode;
    };
    static constexpr uint32_t Capacity = 2;
    static bool Ready(const Request &request);
    static void Prepare(const Request &request); // One preparation step; never call while a player is active.
    static bool Restore(const Request &request, FftPitchCore &voice);
    static void Invalidate(); // Before replacing/reusing sample memory, including incomplete cache copies.
    static uint32_t Hits();
private:
    struct Entry
    {
        Request key = {};
        FftPitchCore::InitialAnalysis data = {};
        bool ready = false;
    };
    static Entry entries[Capacity];
    static FftPitchCore builder;
    static Request pending;
    static bool building;
    static uint32_t next;
    static uint32_t hits;
    static bool Equal(const Request &a, const Request &b);
};
