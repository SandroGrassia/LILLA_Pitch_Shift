#pragma once
#include <cstdint>

// Foreground trim model. Inclusive endpoints as in Lilla; source count >= 2.
class PlaybackRegion
{
public:
    void Reset(uint32_t samples);
    void RotateStep(int delta);
    void ResetStep();
    void ToggleSlice();
    void MoveFrom(int delta);
    void MoveTo(int delta);
    void ResetFrom();
    void MoveCrossMix(int delta);
    uint32_t CrossMix() const;
    uint32_t CrossMixMax() const;
    uint32_t From() const { return first; }
    uint32_t To() const { return last; }
    uint32_t Length() const { return last - first + 1; }
    uint32_t Step() const;
    bool AutoStep() const { return speed == 5; }
    bool Slice() const { return slice; }
private:
    uint32_t cross_mix = 0;
    uint32_t count = 2;
    uint32_t minimum = 2;
    uint32_t first = 0;
    uint32_t last = 1;
    int speed = 5;
    bool slice = false;
};
