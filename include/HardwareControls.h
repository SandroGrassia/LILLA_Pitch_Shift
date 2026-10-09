#pragma once
#include <Arduino.h>
#include <Adafruit_MCP23X17.h>

class HardwareControls
{
public:
    enum Control { Step, From, To, Volume, Count };
    bool Begin();
    void Poll();
    int TakeRotation(Control control);
    bool TakeStepPress();
    bool TakeFromPress();
    bool TakeToPress();

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
    Button buttons[3];
    uint32_t last_scan = 0;
};
