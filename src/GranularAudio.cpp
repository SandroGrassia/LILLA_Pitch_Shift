#include "GranularAudio.h"

GranularAudio::GranularAudio() : AudioStream(0, nullptr)
{
}

void GranularAudio::SetReady()
{
    ready = true;
}

void GranularAudio::TakeDiagnostics(uint32_t &maximum, uint32_t &missed)
{
    maximum = maximum_cycles;
    missed = missed_blocks;
    maximum_cycles = 0;
    missed_blocks = 0;
}

GranularAudio::Levels GranularAudio::TakeLevels()
{
    const Levels result = levels;
    levels = {};
    return result;
}

void GranularAudio::update()
{
    const uint32_t started = ARM_DWT_CYCCNT;
    audio_block_t *block = allocate();
    if (block == nullptr)
    {
        ++missed_blocks;
        if (ready)
        {
            for (uint32_t i = 0; i < AUDIO_BLOCK_SAMPLES; ++i)
            {
                core.Render(); // Keep transport time when the audio pool is exhausted.
            }
        }
    }
    else
    {
        for (uint32_t i = 0; i < AUDIO_BLOCK_SAMPLES; ++i)
        {
            const int32_t reference = ready ? core.ReferenceSample() : 0;
            const int32_t value = ready ? core.Render() : 0;
            block->data[i] = static_cast<int16_t>(value);
            levels.source_energy += static_cast<uint64_t>(static_cast<int64_t>(reference) * reference);
            levels.output_energy += static_cast<uint64_t>(static_cast<int64_t>(value) * value);
            ++levels.samples;
            const uint16_t magnitude = static_cast<uint16_t>(value < 0 ? -value : value);
            if (magnitude > levels.peak)
            {
                levels.peak = magnitude;
            }
        }
        transmit(block);
        release(block);
    }
    const uint32_t elapsed = ARM_DWT_CYCCNT - started;
    if (elapsed > static_cast<uint32_t>(F_CPU_ACTUAL * (AUDIO_BLOCK_SAMPLES / AUDIO_SAMPLE_RATE_EXACT)))
    {
        ++levels.over_budget;
    }
    if (elapsed > maximum_cycles)
    {
        maximum_cycles = elapsed;
    }
}
