#include "FftPitchCore.h"
#include "LegacyFftPitchCore.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

int main()
{
    std::vector<int16_t> source(7019);
    uint32_t random = 1234;
    for (uint32_t i = 0; i < source.size(); ++i)
    {
        random = random * 1664525u + 1013904223u;
        source[i] = static_cast<int16_t>(5000 * std::sin(i * 0.13) + 2000 * std::cos(i * 0.031) + (i % 1300 < 80 ? static_cast<int>(random >> 20) - 2048 : 0));
    }
    FftPitchCore cores[6];
    LegacyFftPitchCore baseline[6];
    for (unsigned voice = 0; voice < 6; ++voice)
    {
        cores[voice].StartNote(source.data(), source.size(), 0.4f + voice * 0.4f, 0.7f);
        baseline[voice].StartNote(source.data(), source.size(), 0.4f + voice * 0.4f, 0.7f);
    }
    for (unsigned block = 0; block < 220; ++block)
    {
        for (unsigned voice = 0; voice < 6; ++voice)
        {
            if (block >= 50 && block % 3 == 0)
            {
                const float pitch = 0.3f + ((block + voice) % 27) * 0.13f;
                cores[voice].SetPitch(pitch);
                baseline[voice].SetPitch(pitch);
            }
            const auto analyses = cores[voice].AnalyzedFrames();
            const auto syntheses = cores[voice].GeneratedHops();
            for (unsigned i = 0; i < 128; ++i)
            {
                // Includes the attack, loop boundaries and bends arriving after preanalysis.
                assert(cores[voice].Render() == baseline[voice].Render());
            }
            assert(cores[voice].AnalyzedFrames() - analyses == (block == 0 || block % 2 == 1 ? 1u : 0u));
            assert(cores[voice].GeneratedHops() - syntheses == (block % 2 == 0 ? 1u : 0u));
        }
    }
    FftPitchCore direct;
    direct.StartNote(source.data(), source.size(), 1.0f, 1.0f);
    float gate = 0.0f;
    for (unsigned i = 0; i < 17000; ++i)
    {
        gate = std::min(1.0f, gate + 1.0f / 221.0f);
        assert(direct.Render() == static_cast<int16_t>(source[i % source.size()] * gate));
    }
    // Exercise transition out of bypass and back to bit-exact direct playback.
    direct.SetPitch(1.2f);
    for (unsigned i = 0; i < 8192; ++i)
    {
        direct.Render();
    }
    direct.SetPitch(1.0f);
    for (unsigned i = 0; i < 8192; ++i)
    {
        direct.Render();
    }
    for (unsigned i = 0; i < 2048; ++i)
    {
        const auto position = direct.Transport();
        assert(direct.Render() == source[position]);
    }
    std::puts("PASS: exact synchronous region-engine output with six voices and bend; alternating analysis/synthesis blocks; direct unity samples and bend return.");
}
