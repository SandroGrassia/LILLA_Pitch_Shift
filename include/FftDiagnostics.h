#pragma once
#include <cstdint>

struct FftCoreDiagnostics
{
    uint32_t renders = 0, clips = 0, nonfinite = 0;
    uint32_t resets = 0, high_fallbacks = 0, low_fallbacks = 0;
    float peak = 0.0f;
};

struct FftVoiceDiagnostics
{
    uint32_t blocks = 0, starts = 0, ends = 0, release_blocks = 0;
    uint32_t source_waits = 0, prepare_waits = 0, schedule_waits = 0;
    uint32_t allocation_failures = 0, unrecorded_reads = 0, near_writer_reads = 0;
    uint32_t full_max_us = 0, gain_clips = 0, output_clips = 0;
    uint32_t read_age_min = UINT32_MAX, read_age_max = 0;
    float gain_peak = 0.0f;
};
