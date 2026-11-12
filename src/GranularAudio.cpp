#include "GranularAudio.h"
#include <cstring>

GranularAudio::GranularAudio(WsolaCore *voices) : AudioStream(0, nullptr), core(voices)
{
    ResetMeasurement();
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

GranularAudio::Levels GranularAudio::TakeLevels(uint32_t scope)
{
    const Levels result = levels[scope];
    levels[scope] = {};
    return result;
}

GranularAudio::Timings GranularAudio::TakeTimings(uint32_t scope)
{
    const Timings result = timings[scope];
    timings[scope] = {};
    return result;
}

void GranularAudio::ResetMeasurement()
{
    for (uint32_t scope = 0; scope <= Mix; ++scope)
    {
        timings[scope] = {};
        levels[scope] = {};
        transient_remaining[scope] = 260;
    }
    maximum_cycles = 0;
    missed_blocks = 0;
}

void GranularAudio::ResetVoiceMeasurement(uint32_t voice)
{
    timings[voice] = {};
    levels[voice] = {};
    transient_remaining[voice] = 260;
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

void GranularAudio::RecordBlock(uint32_t scope, uint32_t engine, uint32_t callback, uint32_t hops, uint32_t budget)
{
    if (transient_remaining[scope] > 0)
    {
        --transient_remaining[scope];
        RecordTiming(timings[scope].transient, engine, callback, hops, budget);
    }
    else if (hops > 0)
    {
        RecordTiming(timings[scope].generate, engine, callback, hops, budget);
    }
    else
    {
        RecordTiming(timings[scope].read, engine, callback, hops, budget);
    }
}

void GranularAudio::RecordLevel(Levels &target, int32_t original, int32_t output)
{
    target.source_energy += static_cast<uint64_t>(static_cast<int64_t>(original) * original);
    target.output_energy += static_cast<uint64_t>(static_cast<int64_t>(output) * output);
    ++target.samples;
    const uint16_t magnitude = static_cast<uint16_t>(output < 0 ? -output : output);
    if (magnitude > target.peak)
    {
        target.peak = magnitude;
    }
}

void GranularAudio::update()
{
    const uint32_t started = ARM_DWT_CYCCNT;
    bool active[VoiceCount];
    uint32_t first[VoiceCount];
    uint32_t engine_elapsed[VoiceCount];
    uint32_t hops[VoiceCount];
    uint32_t total_engine = 0;
    uint32_t total_hops = 0;
    bool any_active = false;
    audio_block_t *block = allocate();
    int16_t *destination = block != nullptr ? block->data : fallback;
    if (block == nullptr)
    {
        ++missed_blocks;
    }
    for (uint32_t voice = 0; voice < VoiceCount; ++voice)
    {
        active[voice] = ready && core[voice].IsActive();
        any_active = any_active || active[voice];
        first[voice] = active[voice] ? core[voice].Transport() : 0;
        const uint32_t hops_before = active[voice] ? core[voice].GeneratedHops() : 0;
        const uint32_t engine_started = ARM_DWT_CYCCNT;
        if (active[voice])
        {
            for (uint32_t i = 0; i < AUDIO_BLOCK_SAMPLES; ++i)
            {
                rendered[voice][i] = core[voice].Render();
            }
        }
        else
        {
            std::memset(rendered[voice], 0, sizeof(rendered[voice]));
        }
        engine_elapsed[voice] = ARM_DWT_CYCCNT - engine_started;
        hops[voice] = active[voice] ? core[voice].GeneratedHops() - hops_before : 0;
        total_engine += active[voice] ? engine_elapsed[voice] : 0u; // Sum only active renderers in this callback.
        total_hops += hops[voice];
    }
    // Meter reads follow all renders, so they do not prewarm voices in this block.
    for (uint32_t voice = 0; voice < VoiceCount; ++voice)
    {
        if (active[voice])
        {
            core[voice].ReadReferenceBlock(reference, first[voice], AUDIO_BLOCK_SAMPLES);
        }
        else
        {
            std::memset(reference, 0, sizeof(reference));
        }
        for (uint32_t i = 0; i < AUDIO_BLOCK_SAMPLES; ++i)
        {
            RecordLevel(levels[voice], reference[i], rendered[voice][i]);
        }
    }
    for (uint32_t i = 0; i < AUDIO_BLOCK_SAMPLES; ++i)
    {
        // Fixed headroom: no level jump in the other voice when one starts or stops.
        int32_t sum = 0;
        for (uint32_t voice = 0; voice < VoiceCount; ++voice)
        {
            sum += rendered[voice][i];
        }
        destination[i] = static_cast<int16_t>(sum / static_cast<int32_t>(VoiceCount));
        RecordLevel(levels[Mix], 0, destination[i]);
    }
    if (block != nullptr)
    {
        transmit(block);
        release(block);
    }
    const uint32_t budget = static_cast<uint32_t>(F_CPU_ACTUAL * (AUDIO_BLOCK_SAMPLES / AUDIO_SAMPLE_RATE_EXACT));
    for (uint32_t voice = 0; voice < VoiceCount; ++voice)
    {
        if (active[voice])
        {
            RecordBlock(voice, engine_elapsed[voice], engine_elapsed[voice], hops[voice], budget);
        }
    }
    // Full callback includes all renderers, meters, mixing and per-voice statistics.
    const uint32_t elapsed = ARM_DWT_CYCCNT - started;
    if (elapsed > budget)
    {
        ++levels[Mix].over_budget;
    }
    if (elapsed > maximum_cycles)
    {
        maximum_cycles = elapsed;
    }
    if (any_active)
    {
        RecordBlock(Mix, total_engine, elapsed, total_hops, budget);
    }
}
