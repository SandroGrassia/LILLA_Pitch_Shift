#include "WsolaCore.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

constexpr double Tau = 6.283185307179586;

double FilterGain(float pitch, double frequency)
{
    WsolaAntialias filter;
    filter.SetPitch(pitch);
    filter.Reset(0.0f);
    double energy = 0.0;
    for (unsigned i = 0; i < 60000; ++i)
    {
        const float sample = filter.Process(static_cast<float>(std::sin(Tau * frequency * i / 44100.0)));
        assert(std::isfinite(sample));
        if (i >= 15900)
        {
            energy += sample * sample;
        }
    }
    return std::sqrt(2.0 * energy / 44100.0);
}

double CoreRms(float pitch, double frequency, bool enabled, uint32_t overlap)
{
    std::vector<int16_t> source(44100);
    for (unsigned i = 0; i < source.size(); ++i)
    {
        source[i] = static_cast<int16_t>(12000.0 * std::sin(Tau * frequency * i / 44100.0));
    }
    WsolaCore core;
    core.SetAntialias(enabled);
    core.SetOverlap(overlap);
    core.StartNote(source.data(), source.size(), pitch, 1.0f);
    double energy = 0.0;
    for (unsigned i = 0; i < 60000; ++i)
    {
        const double sample = core.Render();
        if (i >= 15900)
        {
            energy += sample * sample;
        }
    }
    return std::sqrt(energy / 44100.0);
}

int main()
{
    for (float pitch : {1.1f, 1.5f, 2.0f, 4.0f})
    {
        const double gain = FilterGain(pitch, 1000.0);
        assert(gain > 0.995 && gain < 1.005);
        const double cutoff_gain = FilterGain(pitch, 19845.0 / pitch);
        assert(cutoff_gain > 0.705 && cutoff_gain < 0.709);
    }
    for (uint32_t overlap : {10u, 25u, 50u, 75u})
    {
        for (float pitch : {1.5f, 2.0f, 4.0f})
        {
            const double frequency = pitch == 1.5f ? 20000.0 : pitch == 2.0f ? 16000.0 : 10000.0;
            const double dry = CoreRms(pitch, frequency, false, overlap);
            const double wet = CoreRms(pitch, frequency, true, overlap);
            const double attenuation = 20.0 * std::log10(wet / dry);
            std::printf("overlap=%u pitch=%.1f source_hz=%.0f alias_rms[dry/wet]=%.2f/%.2f attenuation=%.2f dB\n", overlap, pitch, frequency, dry, wet, attenuation);
            assert(dry > 1000.0 && attenuation < -18.0);
        }
    }
    std::vector<int16_t> source(1009);
    for (unsigned i = 0; i < source.size(); ++i)
    {
        source[i] = static_cast<int16_t>(10000.0 * std::sin(i * 0.11));
    }
    // At/below unity the enabled path must be sample-exact with bypass.
    for (float pitch : {0.25f, 0.5f, 1.0f})
    {
        WsolaCore wet;
        WsolaCore dry;
        dry.SetAntialias(false);
        wet.StartNote(source.data(), source.size(), pitch, 1.0f);
        dry.StartNote(source.data(), source.size(), pitch, 1.0f);
        for (unsigned i = 0; i < 10000; ++i)
        {
            assert(wet.Render() == dry.Render());
        }
    }
    WsolaCore wet;
    WsolaCore dry;
    dry.SetAntialias(false);
    wet.SetWindow(220);
    dry.SetWindow(220);
    wet.SetOverlap(75);
    dry.SetOverlap(75);
    wet.StartNote(source.data(), source.size(), 4.0f, 1.0f);
    dry.StartNote(source.data(), source.size(), 4.0f, 1.0f);
    for (unsigned i = 0; i < 10000; ++i)
    {
        wet.Render();
        dry.Render();
    }
    wet.SetAntialias(false);
    for (unsigned i = 0; i < 10000; ++i)
    {
        const int actual = wet.Render();
        const int reference = dry.Render();
        if (i >= 221)
        {
            assert(actual == reference);
        }
    }
    assert(wet.Transport() == dry.Transport() && wet.GeneratedHops() == dry.GeneratedHops());
    // Exercise bend, short-hop boundaries, bypass and geometry transitions on a bounded signal.
    wet.SetAntialias(true);
    for (unsigned i = 0; i < 60000; ++i)
    {
        if (i % 128 == 0)
        {
            wet.SetPitch(0.25f + 3.75f * static_cast<float>((i / 128) % 32) / 31.0f);
        }
        if (i == 20000)
        {
            wet.SetWindow(4410);
            wet.SetOverlap(10);
        }
        assert(std::abs(static_cast<int>(wet.Render())) < 18000);
    }
    wet.SetRunning(false);
    for (unsigned i = 0; i < 300; ++i)
    {
        wet.Render();
    }
    assert(!wet.IsActive());
    WsolaCore fresh;
    fresh.SetWindow(4410);
    fresh.SetOverlap(10);
    wet.StartNote(source.data(), source.size(), 2.0f, 1.0f);
    fresh.StartNote(source.data(), source.size(), 2.0f, 1.0f);
    for (unsigned i = 0; i < 10000; ++i)
    {
        assert(wet.Render() == fresh.Render());
    }
    std::puts("PASS: four-pole response, end-to-end alias attenuation, unity/downpitch transparency, live bypass, bend/geometry transitions and clean note restart.");
}
