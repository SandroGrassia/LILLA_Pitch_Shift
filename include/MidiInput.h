#pragma once
#include <cstdint>

class MidiInput
{
public:
    struct Message
    {
        uint8_t type;
        uint8_t data1;
        uint8_t data2;
    };
    static constexpr uint8_t Capacity = 8;
    Message messages[Capacity];
    void Begin();
    uint8_t Collect(); // Bounded parsing, preserving the order of notes and pitch bends.
};
