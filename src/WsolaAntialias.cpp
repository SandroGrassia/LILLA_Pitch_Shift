#include "WsolaAntialias.h"
#include <algorithm>
#include <cmath>

void WsolaAntialias::SetPitch(float pitch)
{
    pitch = std::max(1.0f, pitch);
    if (pitch == configured_pitch)
    {
        return;
    }
    configured_pitch = pitch;
    const float k = std::tan(3.14159265358979323846f * 0.45f / pitch);
    constexpr float inverse_q[] = {1.847759065f, 0.765366865f};
    for (unsigned i = 0; i < 2; ++i)
    {
        Section &section = sections[i];
        const float norm = 1.0f / (1.0f + inverse_q[i] * k + k * k);
        section.target_b0 = k * k * norm;
        section.target_a1 = 2.0f * (k * k - 1.0f) * norm;
        section.target_a2 = (1.0f - inverse_q[i] * k + k * k) * norm;
    }
}

void WsolaAntialias::Reset(float sample)
{
    for (Section &section : sections)
    {
        section.b0 = section.target_b0;
        section.a1 = section.target_a1;
        section.a2 = section.target_a2;
        section.z1 = (1.0f - section.b0) * sample;
        section.z2 = (section.b0 - section.a2) * sample;
    }
}

float WsolaAntialias::Process(float sample)
{
    for (Section &section : sections)
    {
        section.b0 += (section.target_b0 - section.b0) * (1.0f / 128.0f);
        section.a1 += (section.target_a1 - section.a1) * (1.0f / 128.0f);
        section.a2 += (section.target_a2 - section.a2) * (1.0f / 128.0f);
        const float output = section.b0 * sample + section.z1;
        section.z1 = 2.0f * section.b0 * sample - section.a1 * output + section.z2;
        section.z2 = section.b0 * sample - section.a2 * output;
        sample = output;
    }
    return sample;
}
