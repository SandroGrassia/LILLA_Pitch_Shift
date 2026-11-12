#pragma once
#include <cstdint>

namespace ExperimentConfig
{
extern const char *const SourceFiles[2]; // Identical names share one PSRAM allocation.
constexpr uint8_t RootNote = 60;
constexpr uint8_t MidiChannel = 1;
constexpr float BendSemitones = 2.0f;
}
