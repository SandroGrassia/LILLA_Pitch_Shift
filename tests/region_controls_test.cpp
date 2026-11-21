/*
   LILLA Audio Sampler
   Author: Sandro Grassia (info@lillasampler.it)
   www.lillasampler.it
*/

#include "FftPitchCore.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

int main()
{
    std::vector<int16_t> source(4099);
    for (unsigned i = 0; i < source.size(); ++i)
    {
        source[i] = static_cast<int16_t>(10000 * std::sin(i * 0.21));
    }
    // A bounded loop must be sample-identical to a standalone copy of the region.
    for (unsigned length : {2u, 137u, 1024u, 2500u})
    {
        for (float pitch : {0.5f, 1.0f, 1.7f, 3.0f})
        {
            FftPitchCore bounded;
            FftPitchCore cropped;
            bounded.StartNote(source.data(), source.size(), pitch, 0.5f);
            bounded.SetRegion(333, 333 + length);
            cropped.StartNote(source.data() + 333, length, pitch, 0.5f);
            for (unsigned i = 0; i < 6000; ++i)
            {
                assert(bounded.Render() == cropped.Render());
                assert(bounded.Transport() == 333 + cropped.Transport());
            }
        }
    }
    // Repeated live requests coalesce; old FFT frames cannot survive the reset.
    FftPitchCore live;
    live.StartNote(source.data(), source.size(), 1.0f, 1.0f);
    for (unsigned i = 0; i < 2000; ++i)
    {
        live.Render();
    }
    live.SetRegion(100, 200);
    for (unsigned i = 0; i < 50; ++i)
    {
        live.Render();
    }
    live.SetRegion(3000, 3017);
    for (unsigned i = 0; i < 1400; ++i)
    {
        live.Render();
    }
    assert(live.RegionFrom() == 3000 && live.RegionEnd() == 3017);
    for (unsigned i = 0; i < 1000; ++i)
    {
        const unsigned position = live.Transport();
        assert(position >= 3000 && position < 3017);
        assert(live.Render() == source[position]);
    }
    live.SetRegion(0, 2);
    live.SetRunning(false);
    for (unsigned i = 0; i < 500; ++i)
    {
        live.Render();
    }
    assert(!live.IsActive());

    // Transposed edits must discard spectral and overlap history from outside the region.
    std::vector<int16_t> isolation(4099, 15000);
    std::fill(isolation.begin() + 2000, isolation.begin() + 3000, 0);
    FftPitchCore shifted;
    shifted.StartNote(isolation.data(), isolation.size(), 1.7f, 1.0f);
    for (unsigned i = 0; i < 1000; ++i)
    {
        shifted.Render();
    }
    shifted.SetRegion(2000, 3000);
    for (unsigned i = 0; i < 2000; ++i)
    {
        shifted.Render();
    }
    for (unsigned i = 0; i < 2000; ++i)
    {
        assert(shifted.Render() == 0);
    }
    std::puts("PASS: FFT crop equivalence, tiny loops, region edits and source isolation.");
}
