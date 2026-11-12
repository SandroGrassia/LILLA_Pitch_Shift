#pragma once
#include <cstddef>
#include <cstdint>
#include <deque>

inline uint32_t midi_test_time = 0;
inline uint32_t midi_test_tick = 0;
inline uint32_t micros()
{
    midi_test_time += midi_test_tick;
    return midi_test_time;
}

class HardwareSerial
{
public:
    std::deque<uint8_t> rx;
    uint32_t baud = 0;
    size_t extra = 0;
    unsigned writes = 0;
    void begin(uint32_t value)
    {
        baud = value;
    }
    void addMemoryForRead(void *, size_t size)
    {
        extra = size;
    }
    int available() const
    {
        return static_cast<int>(rx.size());
    }
    uint8_t read()
    {
        const uint8_t value = rx.front();
        rx.pop_front();
        return value;
    }
    void write(uint8_t)
    {
        ++writes;
    }
};
inline HardwareSerial Serial1;
