#include "FftPitchCore.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

int main()
{
    std::setbuf(stdout, nullptr);
    std::vector<int16_t> source(44100);
    for (unsigned frequency : {220u, 997u})
    {
        for (uint32_t i = 0; i < source.size(); ++i)
        {
            source[i] = static_cast<int16_t>(10000.0 * std::sin(i * 6.283185307179586 * frequency / 44100.0));
        }
        for (float pitch : {0.25f, 0.5f, 1.0f, 1.3f, 1.5f, 2.0f, 3.0f, 4.0f})
        {
            FftPitchCore core;
            core.StartNote(source.data(), source.size(), pitch, 1.0f);
            unsigned crossings = 0;
            int previous = 0;
            double energy = 0.0;
            double error = 0.0;
            for (uint32_t i = 0; i < 54100; ++i)
            {
                const int value = core.Render();
                if (i >= 10000)
                {
                    energy += static_cast<double>(value) * value;
                    error += std::abs(value - source[i % source.size()]);
                    if (value > 0 && previous <= 0)
                    {
                        ++crossings;
                    }
                }
                previous = value;
            }
            const double rms = std::sqrt(energy / 44100.0);
            std::printf("FFT tone=%u pitch=%.2f crossings=%u expected=%.2f rms=%.2f unity_error=%.2f\n", frequency, pitch, crossings, frequency * pitch, rms, error / 44100.0);
            assert(std::abs(crossings - frequency * pitch) < frequency * pitch * 0.03 + 3);
            assert(rms > 500.0 && rms < 16000.0);
            if (pitch == 1.0f)
            {
                assert(error / 44100.0 < 5.0);
            }
            assert(core.Transport() == 54100 % source.size());
            assert(core.GeneratedHops() == (54100 + FftPitchCore::Hop - 1) / FftPitchCore::Hop);
            core.SetRunning(false);
            for (unsigned i = 0; i < 300; ++i)
            {
                core.Render();
            }
            assert(!core.IsActive() && core.Render() == 0);
            core.StartNote(source.data(), source.size(), pitch, 1.0f);
            FftPitchCore fresh;
            fresh.StartNote(source.data(), source.size(), pitch, 1.0f);
            for (unsigned i = 0; i < 2048; ++i)
            {
                assert(core.Render() == fresh.Render());
            }
        }
    }
    // Repeated broadband attacks trigger resets; silence and a stationary bin tone do not.
    std::vector<int16_t> attacks(32768, 0);
    uint32_t random = 12345;
    for (unsigned onset : {4096u, 12288u, 20480u})
    {
        for (unsigned i = 0; i < 1024; ++i)
        {
            random = random * 1664525u + 1013904223u;
            attacks[onset + i] = static_cast<int16_t>((static_cast<int>(random >> 16) - 32768) / 4);
        }
    }
    FftPitchCore attack_core;
    FftPitchCore quiet_core;
    std::vector<int16_t> quiet(32768, 0);
    attack_core.StartNote(attacks.data(), attacks.size(), 1.5f, 1.0f);
    quiet_core.StartNote(quiet.data(), quiet.size(), 1.5f, 1.0f);
    for (unsigned i = 0; i < 30000; ++i)
    {
        attack_core.Render();
        assert(quiet_core.Render() == 0);
    }
    std::printf("Attack resets=%u\n", attack_core.TransientResets());
    assert(attack_core.TransientResets() >= 3 && attack_core.TransientResets() <= 6);
    assert(quiet_core.TransientResets() == 0);
    for (unsigned i = 0; i < quiet.size(); ++i)
    {
        quiet[i] = static_cast<int16_t>(8000.0 * std::sin(i * 6.283185307179586 * 16 / 1024));
    }
    quiet_core.StartNote(quiet.data(), quiet.size(), 1.5f, 1.0f);
    for (unsigned i = 0; i < 30000; ++i)
    {
        quiet_core.Render();
    }
    assert(quiet_core.TransientResets() == 0);
    attack_core.StartNote(attacks.data(), attacks.size(), 1.5f, 1.0f);
    assert(attack_core.TransientResets() == 0);
    // Unity reconstruction of a short, discontinuous cyclic source exercises frame wrapping.
    std::vector<int16_t> short_source(137);
    for (unsigned i = 0; i < short_source.size(); ++i)
    {
        short_source[i] = static_cast<int16_t>((i * 2137u) % 16000u - 8000);
    }
    FftPitchCore core;
    core.StartNote(short_source.data(), short_source.size(), 1.0f, 1.0f);
    double error = 0;
    for (unsigned i = 0; i < 10000; ++i)
    {
        const int value = core.Render();
        if (i >= 1024)
        {
            error += std::abs(value - short_source[i % short_source.size()]);
        }
    }
    assert(error / (10000 - 1024) < 8.0);
    // Out-of-band transposition must be removed, not folded down.
    for (unsigned i = 0; i < source.size(); ++i)
    {
        source[i] = static_cast<int16_t>(10000.0 * std::sin(i * 6.283185307179586 * 16000 / 44100.0));
    }
    core.StartNote(source.data(), source.size(), 2.0f, 1.0f);
    double energy = 0;
    for (unsigned i = 0; i < 16000; ++i)
    {
        const double value = core.Render();
        if (i > 2048)
        {
            energy += value * value;
        }
    }
    assert(std::sqrt(energy / (16000 - 2049)) < 100.0);
    for (unsigned i = 0; i < 10000; ++i)
    {
        if (i % 128 == 0)
        {
            core.SetPitch(0.25f + 3.75f * ((i / 128) % 16) / 15.0f);
        }
        core.Render();
    }
    assert(core.Pitch() >= 0.25f && core.Pitch() <= 4.0f);
    std::puts("PASS: FFT pitch, unity reconstruction, fixed tempo, source wrap, fresh restart, band rejection and bend. Host FFT backend; no hardware timing assertion.");
}
