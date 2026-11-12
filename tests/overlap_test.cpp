#include "WsolaCore.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

int main()
{
    std::vector<int16_t> source(1009);
    std::vector<int16_t> dc(1009, 32767);
    for (uint32_t i = 0; i < source.size(); ++i)
    {
        source[i] = static_cast<int16_t>(25000 * std::sin(i * 0.17));
    }
    WsolaCore core;
    WsolaCore envelope;
    core.Attach(source.data(), source.size());
    envelope.Attach(dc.data(), dc.size());
    core.SetRunning(true);
    envelope.SetRunning(true);
    uint32_t total = 0;
    for (uint32_t window : {220u, 882u, 2204u, 4410u})
    {
        core.SetWindow(window);
        envelope.SetWindow(window);
        for (uint32_t overlap : {50u, 75u, 25u, 10u, 50u})
        {
            core.SetOverlap(overlap);
            envelope.SetOverlap(overlap);
            for (uint32_t i = 0; i < 12000; ++i)
            {
                const int value = core.Render();
                const double expected = source[total % source.size()] * (envelope.Render() / 32767.0);
                assert(std::abs(value - expected) < 3.0);
                ++total;
            }
            assert(core.Transport() == total % source.size());
        }
    }
    // Full-scale DC must retain gain with every overlap, pitch and short cyclic source.
    for (int16_t level : {int16_t(32767), int16_t(-32768)})
    {
        std::vector<int16_t> constant(2, level);
        for (uint32_t overlap : {10u, 25u, 50u, 75u})
        {
            for (float pitch : {0.25f, 0.5f, 1.0f, 2.0f, 4.0f})
            {
                WsolaCore flat;
                flat.Attach(constant.data(), constant.size());
                flat.SetVolume(1.0f);
                flat.SetOverlap(overlap);
                flat.SetPitch(pitch);
                flat.SetRunning(true);
                for (uint32_t i = 0; i < 10000; ++i)
                {
                    const int value = flat.Render();
                    if (i > 8000)
                    {
                        assert(std::abs(value - level) <= 3);
                    }
                }
                const uint32_t before = flat.GeneratedHops();
                for (uint32_t i = 0; i < 44100; ++i)
                {
                    flat.Render();
                }
                const uint32_t hop = 882 - 882 * overlap / 100;
                const double expected = 44100.0 * pitch / hop;
                assert(std::abs(static_cast<double>(flat.GeneratedHops() - before) - expected) <= 1.0);
                flat.SetRunning(false);
                for (uint32_t i = 0; i < 300; ++i)
                {
                    flat.Render();
                }
                const uint32_t stopped = flat.Transport();
                assert(flat.Render() == 0 && flat.Transport() == stopped);
                flat.SetRunning(true);
                flat.Render();
                assert(flat.Transport() == 1);
            }
        }
    }
    // Musical pitch must remain correct across all geometries, not just at unity.
    std::vector<int16_t> tone(44100);
    for (uint32_t i = 0; i < tone.size(); ++i)
    {
        tone[i] = static_cast<int16_t>(12000 * std::sin(i * 6.283185307179586 * 997 / 44100));
    }
    for (uint32_t overlap : {10u, 25u, 50u, 75u})
    {
        for (float pitch : {0.5f, 1.3f, 2.0f, 4.0f})
        {
            WsolaCore pitched;
            pitched.Attach(tone.data(), tone.size());
            pitched.SetOverlap(overlap);
            pitched.SetPitch(pitch);
            pitched.SetRunning(true);
            unsigned crossings = 0;
            int previous = 0;
            double energy = 0;
            for (uint32_t i = 0; i < 54100; ++i)
            {
                const int value = pitched.Render();
                if (i >= 10000)
                {
                    energy += static_cast<double>(value) * value;
                    if (value > 0 && previous <= 0)
                    {
                        ++crossings;
                    }
                }
                previous = value;
            }
            const double rms = std::sqrt(energy / 44100);
            std::printf("overlap=%u pitch=%.2f crossings=%u rms=%.1f\n", overlap, pitch, crossings, rms);
            assert(std::abs(crossings - 997.0 * pitch) < 997.0 * pitch * 0.03 + 3);
            assert(rms > 1800 && rms < 2250);
        }
    }
    std::puts("PASS: overlap/window transitions, unity reconstruction, wrap, DC normalization, hop cadence, stop/restart and pitched tones.");
}
