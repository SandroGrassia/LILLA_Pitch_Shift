#include "HardwareControls.h"
#include <SPI.h>

namespace
{
struct Pins
{
    uint8_t chip;
    uint8_t dt;
    uint8_t clk;
};
// Lilla UserInterface.h IDs: Step=13, From=12, To=14, LineOutVol=15.
constexpr Pins pins[HardwareControls::Count] = {{0, 3, 4}, {0, 6, 7}, {0, 1, 0}, {1, 2, 1}};
struct Transition
{
    uint8_t state;
    int8_t rotation;
};
// Lilla green-encoder decoder, with its original rotation polarity.
constexpr Transition transitions[4][8] = { {{7, 0}, {2, 0}, {2, 0}, {2, 0}, {5, 0}, {5, 0}, {5, 0}, {7, 0}}, {{4, 0}, {3, 0}, {3, 0}, {3, 0}, {4, 0}, {4, 0}, {4, 0}, {3, 0}}, {{1, 0}, {1, 0}, {1, 0}, {1, 0}, {6, 0}, {6, 0}, {6, 0}, {6, 0}}, {{0, 0}, {0, 0}, {0, -1}, {0, 0}, {0, 0}, {0, 1}, {0, 0}, {0, 0}} };
}

bool HardwareControls::Begin()
{
    digitalWrite(38, HIGH);
    pinMode(38, OUTPUT);
    digitalWrite(37, HIGH);
    pinMode(37, OUTPUT);
    SPI1.setMOSI(26);
    SPI1.setSCK(27);
    SPI1.setMISO(39);
    if (!chips[0].begin_SPI(37, &SPI1, 0x23))
    {
        return false;
    }
    chips[0].enableAddrPins();
    chips[0].enableAddrPins();
    if (!chips[1].begin_SPI(37, &SPI1, 0x24))
    {
        return false;
    }
    chips[1].enableAddrPins();
    for (const Pins &pin : pins)
    {
        chips[pin.chip].pinMode(pin.dt, INPUT_PULLUP);
        chips[pin.chip].pinMode(pin.clk, INPUT_PULLUP);
    }
    chips[0].pinMode(2, INPUT_PULLUP); // Step pushbutton: chip 3, a2.
    button_raw = chips[0].digitalRead(2);
    button_stable = button_raw;
    return true;
}

void HardwareControls::Poll()
{
    if (micros() - last_scan < 500u)
    {
        return;
    }
    last_scan = micros();
    const uint16_t values[2] = {chips[0].readGPIOAB(), chips[1].readGPIOAB()};
    for (uint8_t i = 0; i < Count; ++i)
    {
        const Pins &pin = pins[i];
        const uint8_t read = ((values[pin.chip] >> pin.dt) & 1u) | (((values[pin.chip] >> pin.clk) & 1u) << 1);
        const Transition &transition = transitions[read][states[i]];
        rotations[i] += transition.rotation;
        states[i] = transition.state;
    }
    const bool pressed_level = (values[0] & (1u << 2)) != 0;
    if (pressed_level != button_raw)
    {
        button_raw = pressed_level;
        button_changed = millis();
    }
    if (button_stable != button_raw && millis() - button_changed >= 25u)
    {
        button_stable = button_raw;
        if (!button_stable)
        {
            step_pressed = true;
        }
    }
}

int HardwareControls::TakeRotation(Control control)
{
    const int value = rotations[control];
    rotations[control] = 0;
    return value;
}

bool HardwareControls::TakeStepPress()
{
    const bool value = step_pressed;
    step_pressed = false;
    return value;
}
