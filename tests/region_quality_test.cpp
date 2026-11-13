#include "FftPitchCore.h"
#include "LegacyFftPitchCore.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

struct Measure { double rms; double purity; };

template<class Core> Measure MeasureTone(unsigned frequency, float pitch)
{
    std::vector<int16_t> source(44100);
    for (unsigned i = 0; i < source.size(); ++i)
    {
        source[i] = static_cast<int16_t>(10000 * std::sin(6.283185307179586 * frequency * i / 44100));
    }
    Core core;
    core.StartNote(source.data(), source.size(), pitch, 1.0f);
    double energy = 0;
    double sine = 0;
    double cosine = 0;
    for (unsigned i = 0; i < 54100; ++i)
    {
        const double value = core.Render();
        if (i >= 10000)
        {
            const double angle = 6.283185307179586 * frequency * pitch * i / 44100;
            energy += value * value;
            sine += value * std::sin(angle);
            cosine += value * std::cos(angle);
        }
    }
    return {std::sqrt(energy / 44100), 2 * (sine * sine + cosine * cosine) / (44100 * energy)};
}

int main()
{
    for (unsigned frequency : {220u, 997u})
    {
        for (float pitch : {0.5f, 1.0f, 1.5f, 2.0f, 3.0f})
        {
            const auto old = MeasureTone<LegacyFftPitchCore>(frequency, pitch);
            const auto current = MeasureTone<FftPitchCore>(frequency, pitch);
            std::printf("tone=%u pitch=%.1f old/new RMS=%.1f/%.1f target-energy=%.5f/%.5f\n", frequency, pitch, old.rms, current.rms, old.purity, current.purity);
            assert(current.rms > 5500 && current.rms < 8000);
            assert(current.purity > 0.97);
            if (pitch >= 2.0f)
            {
                assert(current.purity > old.purity);
            }
        }
    }
    std::puts("PASS: region mapping tone level and spectral concentration at pitch 0.5..3; no speech-quality or hardware-cost claim.");
}
