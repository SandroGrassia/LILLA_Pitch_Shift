#include "GranularCore.h"
#include <cmath>

void GranularCore::Attach(const int16_t *samples, uint32_t count)
{
    source = samples;
    source_count = count;
}

void GranularCore::SetPitch(float value)
{
    pitch_target = value;
}

void GranularCore::SetGrain(uint32_t samples, float slope)
{
    if (samples != grain_length && initialized)
    {
        geometry_pending = true;
    }
    grain_length = samples;
    grain_slope = slope;
}

void GranularCore::SetGrainCount(uint32_t value)
{
    if (value != requested_count && initialized)
    {
        geometry_pending = true;
    }
    requested_count = value;
}

void GranularCore::SetVolume(float value)
{
    volume_target = value;
}

void GranularCore::SetRunning(bool value)
{
    running = value;
}

uint32_t GranularCore::Transport() const
{
    return transport;
}

float GranularCore::Pitch() const
{
    return pitch_now;
}

void GranularCore::Restart(Grain &grain)
{
    grain.index = transport;
    grain.fraction = 0.0f;
    grain.age = 0;
    grain.length = grain_length;
    grain.ramp = grain_length * grain_slope;
}

void GranularCore::Prime()
{
    active_count = requested_count;
    for (uint32_t i = 0; i < active_count; ++i)
    {
        Grain &grain = grains[i];
        Restart(grain);
        grain.age = i * grain_length / active_count;
        // Birth position is transport - age; elapsed playback advances by pitch * age.
        const float offset = grain.age * (pitch_now - 1.0f);
        const int64_t whole = static_cast<int64_t>(std::floor(offset));
        const int64_t wrapped = ((static_cast<int64_t>(transport) + whole) % source_count + source_count) % source_count;
        grain.index = static_cast<uint32_t>(wrapped);
        grain.fraction = offset - static_cast<float>(whole);
    }
    initialized = true;
}

int16_t GranularCore::Render()
{
    if (!running && gate == 0.0f)
    {
        initialized = false;
        geometry_pending = false;
        return 0;
    }
    if (!initialized)
    {
        transport = 0;
        pitch_now = pitch_target;
        Prime();
    }
    pitch_now += (pitch_target - pitch_now) * (1.0f / 256.0f);
    volume_now += (volume_target - volume_now) * (1.0f / 256.0f);
    if (running && !geometry_pending)
    {
        gate = std::fmin(1.0f, gate + 1.0f / 221.0f);
    }
    else
    {
        gate = std::fmax(0.0f, gate - 1.0f / 221.0f);
    }
    if (geometry_pending && gate == 0.0f)
    {
        Prime(); // Restore evenly staggered grains at the current transport position.
        geometry_pending = false;
    }
    float sum = 0.0f;
    float weights = 0.0f;
    for (uint32_t i = 0; i < active_count; ++i)
    {
        Grain &grain = grains[i];
        const float attack = grain.age / grain.ramp;
        const float release = (grain.length - 1u - grain.age) / grain.ramp;
        const float weight = std::fmax(0.0f, std::fmin(1.0f, std::fmin(attack, release)));
        const uint32_t next = grain.index + 1u == source_count ? 0u : grain.index + 1u;
        const float value = source[grain.index] + grain.fraction * (static_cast<float>(source[next]) - source[grain.index]);
        sum += weight * value;
        weights += weight;
        const float advanced = grain.fraction + pitch_now;
        const uint32_t whole = static_cast<uint32_t>(advanced);
        grain.fraction = advanced - whole;
        grain.index += whole;
        while (grain.index >= source_count)
        {
            grain.index -= source_count;
        }
    }
    ++transport;
    if (transport == source_count)
    {
        transport = 0;
    }
    for (uint32_t i = 0; i < active_count; ++i)
    {
        Grain &grain = grains[i];
        ++grain.age;
        if (grain.age == grain.length)
        {
            Restart(grain);
        }
    }
    const float output = (weights > 0.0f ? sum / weights : 0.0f) * volume_now * gate;
    return static_cast<int16_t>(std::fmax(-32768.0f, std::fmin(32767.0f, output)));
}
