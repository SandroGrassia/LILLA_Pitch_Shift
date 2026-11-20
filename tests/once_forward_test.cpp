#include "FftPitchCore.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

int main()
{
    std::vector<int16_t> source(4099, 20000);
    for (unsigned i = 300; i < 2300; ++i)
    {
        source[i] = static_cast<int16_t>(10000 * std::sin(i * 0.2));
    }
    for (unsigned length : {2u, 100u, 1024u, 2000u})
    {
        for (float pitch : {0.5f, 1.0f, 1.7f, 3.0f})
        {
            FftPitchCore once;
            FftPitchCore unmixed;
            once.StartNote(source.data(), source.size(), pitch, 0.5f);
            unmixed.StartNote(source.data(), source.size(), pitch, 0.5f);
            once.SetRegion(300, 300 + length, length > 2 ? 2 : 0);
            unmixed.SetRegion(300, 300 + length);
            once.SetMode(FftPitchCore::PlayMode::OnceForward);
            unmixed.SetMode(FftPitchCore::PlayMode::OnceForward);
            for (unsigned i = 0; i < length; ++i)
            {
                assert(once.IsActive());
                assert(once.Transport() == 300 + i);
                assert(once.Render() == unmixed.Render());
            }
            assert(!once.IsActive() && once.Transport() == 300 + length);
            for (unsigned i = 0; i < 2000; ++i)
            {
                assert(once.Render() == 0);
            }
        }
    }
    // No wrapping/lookahead leakage: a silent selected region has a loud file around it.
    std::fill(source.begin() + 300, source.begin() + 2300, 0);
    FftPitchCore quiet;
    quiet.StartNote(source.data(), source.size(), 1.7f, 1.0f);
    quiet.SetRegion(300, 2300);
    quiet.SetMode(FftPitchCore::PlayMode::OnceForward);
    for (unsigned i = 0; i < 4000; ++i)
    {
        assert(quiet.Render() == 0);
    }
    FftPitchCore live;
    live.StartNote(source.data(), source.size(), 1.0f, 1.0f);
    for (unsigned i = 0; i < 500; ++i)
    {
        live.Render();
    }
    live.SetMode(FftPitchCore::PlayMode::OnceForward);
    for (unsigned i = 0; i < 300; ++i)
    {
        live.Render();
    }
    assert(live.Mode() == FftPitchCore::PlayMode::OnceForward && live.Transport() == 800);
    live.SetMode(FftPitchCore::PlayMode::LoopForward);
    for (unsigned i = 0; i < 6000; ++i)
    {
        live.Render();
    }
    assert(live.IsActive() && live.Mode() == FftPitchCore::PlayMode::LoopForward);
    live.SetMode(FftPitchCore::PlayMode::OnceForward);
    live.SetRunning(false);
    for (unsigned i = 0; i < 1000; ++i)
    {
        live.Render();
    }
    assert(!live.IsActive());
    std::puts("PASS: forward one-shot duration, zero-padded reads, mode changes and release.");
}
