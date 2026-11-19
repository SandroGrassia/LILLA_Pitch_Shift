#pragma once
#include <cstdint>

namespace ExperimentConfig
{
extern const char SourceFile[]; // Raw mono PCM filename in SPI Flash.
constexpr uint8_t RootNote = 60;
constexpr uint8_t MidiChannel = 1;
constexpr float BendSemitones = 2.0f;
}
