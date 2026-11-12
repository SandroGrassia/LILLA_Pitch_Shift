#include <Arduino.h>
#include "MidiInput.h"
#include <cassert>
#include <cstdio>

int main()
{
    MidiInput input;
    input.Begin();
    assert(Serial1.baud == 31250 && Serial1.extra == 512);
    Serial1.rx = {0x90, 60};
    assert(input.Collect() == 0);
    Serial1.rx.push_back(127);
    assert(input.Collect() == 1);
    assert(input.messages[0].type == 0x90 && input.messages[0].data1 == 60 && input.messages[0].data2 == 127);
    // Running status, velocity-zero release and real-time bytes interleaved with a message.
    Serial1.rx = {61, 0xF8, 100, 60, 0, 0xE0, 127, 127, 0x80, 61, 0};
    assert(input.Collect() == 4);
    assert(input.messages[0].type == 0x90 && input.messages[0].data1 == 61);
    assert(input.messages[1].type == 0x80 && input.messages[1].data1 == 60);
    assert(input.messages[2].type == 0xE0 && input.messages[2].data1 == 127 && input.messages[2].data2 == 127);
    assert(input.messages[3].type == 0x80 && input.messages[3].data1 == 61);
    Serial1.rx = {0x91, 60, 127, 0xE1, 0, 0, 0xB0, 123, 0};
    assert(input.Collect() == 1);
    assert(input.messages[0].type == 0xB0 && input.messages[0].data1 == 123);
    for (uint8_t i = 0; i < 12; ++i)
    {
        Serial1.rx.push_back(0x90);
        Serial1.rx.push_back(60 + i);
        Serial1.rx.push_back(100);
    }
    assert(input.Collect() == 8);
    assert(Serial1.rx.size() == 12);
    assert(input.messages[7].data1 == 67);
    assert(input.Collect() == 4 && input.messages[0].data1 == 68);
    Serial1.rx.assign(80, 0xF8);
    assert(input.Collect() == 0 && Serial1.rx.size() == 16);
    midi_test_tick = 20;
    assert(input.Collect() == 0 && Serial1.rx.size() == 13);
    assert(Serial1.writes == 0);
    std::puts("PASS: actual MIDI library DIN parsing, baud, channel filter, partial messages, running status, zero-velocity NoteOff, ordered pitch bend, byte/message/time bounds and disabled Thru.");
}
