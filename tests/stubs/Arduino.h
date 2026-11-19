#pragma once
#include <cstdint>
inline uint32_t test_cycles = 0;
inline uint32_t TestCycleCounter()
{
    return ++test_cycles;
}
#define ARM_DWT_CYCCNT TestCycleCounter()
constexpr uint32_t F_CPU_ACTUAL = 600000000;
