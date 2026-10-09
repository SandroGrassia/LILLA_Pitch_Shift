#include "WsolaCore.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

struct Measurement
{
    double rms;
    uint32_t crossings;
};

Measurement Measure(float pitch, unsigned frequency, unsigned radius)
{
    std::vector<int16_t> source(44100);
    for (uint32_t i = 0; i < source.size(); ++i)
    {
        source[i] = static_cast<int16_t>(12000.0 * std::sin(i * 6.283185307179586 * frequency / 44100.0));
    }
    WsolaCore core;
    core.Attach(source.data(), source.size());
    core.SetWindow(2204u);
    core.SetPitch(pitch);
    core.SetSearch(radius);
    core.SetRunning(true);
    double energy = 0.0;
    uint32_t crossings = 0;
    int previous = 0;
    for (uint32_t i = 0; i < 100000u; ++i)
    {
        const int output = core.Render();
        if (i >= 11800u)
        {
            energy += static_cast<double>(output) * output;
            if (previous <= 0 && output > 0)
            {
                ++crossings;
            }
        }
        previous = output;
    }
    assert(core.Transport() == 100000u % source.size());
    return {std::sqrt(energy / 88200.0), crossings};
}

int main()
{
    std::vector<int16_t> source(1009);
    std::vector<int16_t> dc(source.size(), 32767);
    for (uint32_t i = 0; i < source.size(); ++i)
    {
        source[i] = static_cast<int16_t>(12000.0 * std::sin(i * 0.17));
    }
    WsolaCore core;
    WsolaCore envelope;
    core.Attach(source.data(), source.size());
    envelope.Attach(dc.data(), dc.size());
    core.SetRunning(true);
    envelope.SetRunning(true);
    uint32_t total = 0;
    for (uint32_t length : {2204u, 220u, 882u, 4410u})
    {
        core.SetWindow(length);
        envelope.SetWindow(length);
        for (uint32_t i = 0; i < 10000u; ++i)
        {
            const int output = core.Render();
            const double expected = source[total % source.size()] * (envelope.Render() / 32767.0);
            assert(std::fabs(output - expected) <= 2.0);
            ++total;
        }
    }
    assert(core.Transport() == total % source.size());
    for (uint32_t i = 0; i < 30000u; ++i)
    {
        core.SetPitch(i < 15000u ? 4.0f : 0.25f);
        core.SetSearch(i % (WsolaCore::MaxSearch + 1u));
        assert(std::abs(static_cast<int>(core.Render())) <= 3001);
        ++total;
    }
    assert(core.Transport() == total % source.size());
    core.SetWindow(220u);
    core.SetPitch(4.0f);
    core.SetSearch(WsolaCore::MaxSearch);
    for (uint32_t i = 0; i < 3000u; ++i)
    {
        assert(std::abs(static_cast<int>(core.Render())) <= 3001);
        ++total;
    }
    assert(core.Transport() == total % source.size());
    core.SetRunning(false);
    for (uint32_t i = 0; i < 300u; ++i)
    {
        core.Render();
    }
    const uint32_t stopped = core.Transport();
    for (uint32_t i = 0; i < 100u; ++i)
    {
        assert(core.Render() == 0);
    }
    assert(core.Transport() == stopped);
    core.SetRunning(true);
    core.Render();
    assert(core.Transport() == 1u);
    for (unsigned frequency : {67u, 997u})
    {
        for (float ratio : {0.25f, 0.5f, 1.0f, 1.3f, 2.0f, 4.0f})
        {
            const Measurement result = Measure(ratio, frequency, 441u);
            const double expected_crossings = frequency * ratio * 2.0;
            std::printf("tone=%u pitch=%.2f rms=%.1f crossings=%u expected=%.1f\n", frequency, ratio, result.rms, result.crossings, expected_crossings);
            assert(std::fabs(result.crossings - expected_crossings) < expected_crossings * 0.03 + 3.0);
            assert(result.rms > 1800.0 && result.rms < 2250.0);
        }
    }
    const Measurement unaligned = Measure(0.5f, 67u, 0u);
    const Measurement aligned = Measure(0.5f, 67u, 441u);
    std::printf("67 Hz at pitch 0.5: RMS search-off %.1f; search-on %.1f\n", unaligned.rms, aligned.rms);
    assert(aligned.rms > unaligned.rms * 1.15);
    std::puts("PASS: unity reconstruction including startup/window changes, wrapping, pitch sweep, fixed tempo, stop/restart, low/high tone pitch and RMS, alignment benefit.");
}
