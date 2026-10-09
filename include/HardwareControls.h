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

private:
    Adafruit_MCP23X17 chips[2];
    uint8_t states[Count] = {};
    int rotations[Count] = {};
    bool button_raw = true;
    bool button_stable = true;
    bool step_pressed = false;
    uint32_t button_changed = 0;
    uint32_t last_scan = 0;
};
