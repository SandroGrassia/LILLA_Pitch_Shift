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
            block->data[i] = ready ? core.Render() : 0;
        }
        transmit(block);
        release(block);
    }
    const uint32_t elapsed = ARM_DWT_CYCCNT - started;
    if (elapsed > maximum_cycles)
    {
        maximum_cycles = elapsed;
    }
}
