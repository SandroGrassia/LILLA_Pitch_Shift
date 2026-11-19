#include "GranularCore.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

int main()
{
    std::vector<int16_t> source(1009);
    for (uint32_t i = 0; i < source.size(); ++i)
    {
        source[i] = static_cast<int16_t>(12000.0 * std::sin(i * 0.17));
    }
    // Independent birth-time model, including partially elapsed grains at startup.
    for (float ratio : {0.25f, 0.5f, 1.0f, 1.3f, 2.0f, 4.0f})
    {
        GranularCore primed;
        primed.Attach(source.data(), source.size());
        primed.SetPitch(ratio);
        primed.SetGrain(2204u, 0.25f);
        primed.SetRunning(true);
        for (int sample = 0; sample < 1800; ++sample)
        {
            double sum = 0.0;
            double weights = 0.0;
            for (int grain = 0; grain < 4; ++grain)
            {
                int birth = -grain * 551;
                while (sample - birth >= 2204)
                {
                    birth += 2204;
                }
                const int age = sample - birth;
                const double weight = std::fmax(0.0, std::fmin(1.0, std::fmin(age / 551.0, (2203 - age) / 551.0)));
                const double position = std::fmod(std::fmod(birth + static_cast<double>(ratio) * age, source.size()) + source.size(), source.size());
                const uint32_t index = static_cast<uint32_t>(position);
                const double fraction = position - index;
                const double value = source[index] + fraction * (source[(index + 1u) % source.size()] - source[index]);
                sum += value * weight;
                weights += weight;
            }
            const double expected = sum / weights * 0.25 * std::fmin(1.0, (sample + 1) / 221.0);
            assert(std::fabs(primed.Render() - expected) <= 3.0);
        }
    }
    // A DC companion measures only the gain envelope: compare reconstruction even inside fades.
    std::vector<int16_t> dc(source.size(), 32767);
    GranularCore startup;
    GranularCore envelope;
    startup.Attach(source.data(), source.size());
    envelope.Attach(dc.data(), dc.size());
    startup.SetRunning(true);
    envelope.SetRunning(true);
    for (uint32_t count : {4u, 16u, 2u, 8u})
    {
        for (uint32_t length : {4408u, 220u, 2204u, 880u})
        {
            startup.SetGrainCount(count);
            envelope.SetGrainCount(count);
            startup.SetGrain(length, 0.25f);
            envelope.SetGrain(length, 0.25f);
            for (uint32_t i = 0; i < 2000u; ++i)
            {
                const uint32_t position = startup.Transport();
                const int output = startup.Render();
                const double expected = source[position] * (envelope.Render() / 32767.0);
                assert(std::fabs(output - expected) <= 2.0);
            }
        }
    }
    for (uint32_t length : {220u, 880u, 2204u, 4408u})
    {
        for (float slope : {0.05f, 0.25f, 0.5f})
        {
            GranularCore core;
            core.Attach(source.data(), source.size());
            core.SetVolume(1.0f);
            core.SetGrain(length, slope);
            core.SetRunning(true);
            for (uint32_t i = 0; i < 50000; ++i)
            {
                const int16_t output = core.Render();
                if (i > 5000u)
                {
                    assert(std::abs(static_cast<int>(output) - source[i % source.size()]) <= 2);
                }
            }
            assert(core.Transport() == 50000u % source.size());
            core.SetRunning(false);
            for (uint32_t i = 0; i < 300; ++i)
            {
                core.Render();
            }
            const uint32_t stopped = core.Transport();
            for (uint32_t i = 0; i < 1000; ++i)
            {
                assert(core.Render() == 0);
            }
            assert(core.Transport() == stopped);
            core.SetRunning(true);
            core.Render();
            assert(core.Transport() == 1u);
        }
    }
    GranularCore density;
    density.Attach(source.data(), source.size());
    density.SetVolume(1.0f);
    density.SetRunning(true);
    for (uint32_t count : {4u, 8u, 16u, 2u, 4u, 16u, 2u})
    {
        density.SetGrainCount(count);
        for (uint32_t i = 0; i < 10000u; ++i)
        {
            const uint32_t position = density.Transport();
            const int16_t output = density.Render();
            if (i > 5000u)
            {
                assert(std::abs(static_cast<int>(output) - source[position]) <= 2);
            }
        }
    }
    assert(density.Transport() == 70000u % source.size());
    density.SetRunning(false);
    for (uint32_t i = 0; i < 300u; ++i)
    {
        density.Render();
    }
    density.SetGrainCount(16u);
    density.SetRunning(true);
    for (uint32_t i = 0; i < 10000u; ++i)
    {
        const int16_t output = density.Render();
        if (i > 5000u)
        {
            assert(std::abs(static_cast<int>(output) - source[i % source.size()]) <= 2);
        }
    }
    std::vector<int16_t> tone(55u * 64u);
    for (uint32_t i = 0; i < tone.size(); ++i)
    {
        tone[i] = static_cast<int16_t>(16000.0 * std::sin(i * 6.283185307179586 / 55.0));
    }
    for (float ratio : {0.25f, 0.5f, 1.0f, 2.0f, 4.0f})
    {
        GranularCore shifted;
        shifted.Attach(tone.data(), tone.size());
        shifted.SetPitch(ratio);
        shifted.SetVolume(1.0f);
        shifted.SetGrain(880u, 0.25f);
        shifted.SetRunning(true);
        int previous = 0;
        uint32_t crossings = 0;
        for (uint32_t i = 0; i < 50000u; ++i)
        {
            const int output = shifted.Render();
            if (i >= 5000u && previous <= 0 && output > 0)
            {
                ++crossings;
            }
            previous = output;
        }
        assert(std::fabs(crossings - 45000.0f * ratio / 55.0f) <= 2.0f);
        assert(shifted.Transport() == 50000u % tone.size());
    }
    std::vector<int16_t> constant(7, 10000);
    GranularCore core;
    core.Attach(constant.data(), constant.size());
    core.SetVolume(1.0f);
    core.SetRunning(true);
    for (uint32_t i = 0; i < 100000; ++i)
    {
        if (i == 20000u)
        {
            core.SetPitch(4.0f);
            core.SetGrain(220u, 0.5f);
        }
        if (i == 40000u)
        {
            core.SetPitch(0.25f);
            core.SetGrain(4408u, 0.05f);
        }
        if (i == 60000u)
        {
            core.SetPitch(1.0f);
            core.SetGrain(880u, 0.25f);
        }
        const int16_t output = core.Render();
        assert(output >= 0 && output <= 10000);
        if (i > 5000u && !(i >= 20000u && i < 21000u) && !(i >= 40000u && i < 41000u) && !(i >= 60000u && i < 61000u))
        {
            assert(output >= 9998 && output <= 10000);
        }
    }
    for (uint32_t length : {220u, 880u, 2204u, 4408u, 880u, 220u})
    {
        core.SetGrain(length, 0.25f);
        for (uint32_t i = 0; i < 5000u; ++i)
        {
            const int16_t output = core.Render();
            if (i > 1000u)
            {
                assert(output >= 9998 && output <= 10000);
            }
        }
    }
    assert(core.Transport() == 130000u % constant.size());
    assert(std::fabs(core.Pitch() - 1.0f) < 0.0001f);
    std::puts("PASS: grain counts 2/4/8/16, live density transitions and stopped selection; measured tone pitch ratios 0.25/0.5/1/2/4; unity reconstruction; all durations/slopes; long grains across short source wraps; normalized DC after all grain transitions including 20-to-5 ms; bounded geometry fades; fixed tempo; stop fade and restart.");
}
