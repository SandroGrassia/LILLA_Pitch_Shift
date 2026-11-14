#pragma once
#include <Arduino.h>
#include <Adafruit_MCP23X17.h>

class HardwareControls
{
public:
    enum Control { Step, From, To, Volume, FileSelect, CrossMix, Count };
    bool Begin();
    void Poll();
    int TakeRotation(Control control);
    bool TakeStepPress();
    bool TakeFromPress();
    bool TakeToPress();
    bool TakeModePress(); // R1 physical Select button (Lilla ID 8).

private:
    Adafruit_MCP23X17 chips[2];
    uint8_t states[Count] = {};
    int rotations[Count] = {};
    struct Button
    {
        bool raw = true;
        bool stable = true;
        bool pressed = false;
        uint32_t changed = 0;
    };
    Button buttons[4];
    uint32_t last_scan = 0;
};
