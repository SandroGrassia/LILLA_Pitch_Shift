#include "GranularAudio.h"
#include <cstring>

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

GranularAudio::Timings GranularAudio::TakeTimings()
{
    const Timings result = timings;
    timings = {};
    return result;
}

void GranularAudio::ResetMeasurement()
{
    timings = {};
    levels = {};
    transient_remaining = 260;
    maximum_cycles = 0;
    missed_blocks = 0;
}

void GranularAudio::RecordTiming(Timing &target, uint32_t engine, uint32_t callback, uint32_t hops, uint32_t budget)
{
    ++target.blocks;
    target.engine_cycles += engine;
    target.callback_cycles += callback;
    target.hops += hops;
    if (engine < target.engine_min)
    {
        target.engine_min = engine;
    }
    if (engine > target.engine_max)
    {
        target.engine_max = engine;
    }
    if (callback > target.callback_max)
    {
        target.callback_max = callback;
    }
    if (hops > target.hops_max)
    {
        target.hops_max = hops;
    }
    if (callback > budget)
    {
        ++target.late;
    }
}

void GranularAudio::update()
{
    const uint32_t started = ARM_DWT_CYCCNT;
    const bool active = ready && core.IsActive();
    const uint32_t first = core.Transport();
    const uint32_t hops_before = core.GeneratedHops();
    audio_block_t *block = allocate();
    int16_t *destination = block != nullptr ? block->data : fallback;
    if (block == nullptr)
    {
        ++missed_blocks;
    }
    const uint32_t engine_started = ARM_DWT_CYCCNT;
    if (ready)
    {
        for (uint32_t i = 0; i < AUDIO_BLOCK_SAMPLES; ++i)
        {
            destination[i] = core.Render();
        }
    }
    else
    {
        std::memset(destination, 0, AUDIO_BLOCK_SAMPLES * sizeof(int16_t));
    }
    const uint32_t engine_elapsed = ARM_DWT_CYCCNT - engine_started;
    const uint32_t hops = core.GeneratedHops() - hops_before;
    // Reference reads follow rendering, so this meter does not prewarm its source region.
    if (active)
    {
        core.ReadReferenceBlock(reference, first, AUDIO_BLOCK_SAMPLES);
    }
    else
    {
        std::memset(reference, 0, sizeof(reference));
    }
    for (uint32_t i = 0; i < AUDIO_BLOCK_SAMPLES; ++i)
    {
        const int32_t original = reference[i];
        const int32_t value = destination[i];
        levels.source_energy += static_cast<uint64_t>(static_cast<int64_t>(original) * original);
        levels.output_energy += static_cast<uint64_t>(static_cast<int64_t>(value) * value);
        ++levels.samples;
        const uint16_t magnitude = static_cast<uint16_t>(value < 0 ? -value : value);
        if (magnitude > levels.peak)
        {
            levels.peak = magnitude;
        }
    }
    if (block != nullptr)
    {
        transmit(block);
        release(block);
    }
    const uint32_t elapsed = ARM_DWT_CYCCNT - started;
    const uint32_t budget = static_cast<uint32_t>(F_CPU_ACTUAL * (AUDIO_BLOCK_SAMPLES / AUDIO_SAMPLE_RATE_EXACT));
    if (elapsed > budget)
    {
        ++levels.over_budget;
    }
    if (elapsed > maximum_cycles)
    {
        maximum_cycles = elapsed;
    }
    if (active)
    {
        if (transient_remaining > 0)
        {
            --transient_remaining;
            RecordTiming(timings.transient, engine_elapsed, elapsed, hops, budget);
        }
        else if (hops > 0)
        {
            RecordTiming(timings.generate, engine_elapsed, elapsed, hops, budget);
        }
        else
        {
            RecordTiming(timings.read, engine_elapsed, elapsed, hops, budget);
        }
    }
}
