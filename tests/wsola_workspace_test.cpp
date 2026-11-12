#include "WsolaCore.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

void ChangeControls(WsolaCore &core, unsigned voice, unsigned block, const std::vector<int16_t> &source)
{
    constexpr float pitches[] = {0.25f, 0.5f, 0.9f, 1.0f, 1.3f, 2.0f, 4.0f};
    constexpr uint32_t overlaps[] = {10, 25, 50, 75};
    constexpr uint32_t windows[] = {220, 882, 4410};
    if (block % 31 == 0)
    {
        core.SetPitch(pitches[(voice + block / 31) % 7]);
    }
    if (block % 19 == 0)
    {
        core.SetOverlap(overlaps[(voice + block / 19) % 4]);
    }
    if (block % 37 == 0)
    {
        core.SetWindow(windows[(voice + block / 37) % 3]);
    }
    if (block % 23 == 0)
    {
        core.SetAntialias(((voice + block / 23) & 1u) != 0);
    }
    if (block == 150 + voice)
    {
        core.SetRunning(false);
    }
    if (block == 166 + voice)
    {
        core.StartNote(source.data(), source.size(), pitches[voice % 7], 0.5f);
    }
}

int main()
{
    constexpr unsigned Voices = 12;
    auto serial = std::make_unique<WsolaCore[]>(Voices);
    auto interleaved = std::make_unique<WsolaCore[]>(Voices);
    std::vector<int16_t> sources[Voices];
    for (unsigned voice = 0; voice < Voices; ++voice)
    {
        sources[voice].resize(137 + voice * 173);
        for (unsigned i = 0; i < sources[voice].size(); ++i)
        {
            sources[voice][i] = static_cast<int16_t>(12000.0 * std::sin(i * (0.11 + voice * 0.017)) + 3000.0 * std::cos(i * 0.73));
        }
        serial[voice].StartNote(sources[voice].data(), sources[voice].size(), 1.0f, 0.5f);
        interleaved[voice].StartNote(sources[voice].data(), sources[voice].size(), 1.0f, 0.5f);
        serial[voice].SetSearch(voice % 2 == 0 ? 882 : 441);
        interleaved[voice].SetSearch(voice % 2 == 0 ? 882 : 441);
    }
    uint64_t hash = 14695981039346656037ull;
    int16_t expected[Voices][128];
    for (unsigned block = 0; block < 200; ++block)
    {
        for (unsigned voice = 0; voice < Voices; ++voice)
        {
            ChangeControls(serial[voice], voice, block, sources[voice]);
            ChangeControls(interleaved[voice], voice, block, sources[voice]);
            for (unsigned i = 0; i < 128; ++i)
            {
                expected[voice][i] = serial[voice].Render();
                hash = (hash ^ static_cast<uint16_t>(expected[voice][i])) * 1099511628211ull;
            }
        }
        for (unsigned i = 0; i < 128; ++i)
        {
            for (unsigned reverse = Voices; reverse > 0; --reverse)
            {
                const unsigned voice = reverse - 1;
                assert(interleaved[voice].Render() == expected[voice][i]);
            }
        }
        for (unsigned voice = 0; voice < Voices; ++voice)
        {
            assert(interleaved[voice].Transport() == serial[voice].Transport());
            assert(interleaved[voice].GeneratedHops() == serial[voice].GeneratedHops());
            assert(interleaved[voice].Offset() == serial[voice].Offset());
            assert(interleaved[voice].Correlation() == serial[voice].Correlation());
            hash = (hash ^ serial[voice].GeneratedHops()) * 1099511628211ull;
            hash = (hash ^ static_cast<uint32_t>(serial[voice].Offset())) * 1099511628211ull;
        }
    }
    std::printf("PASS: 12 voices, per-block versus interleaved rendering, independent sources/geometries, wrap, bend, AA and restart; fingerprint=%016llx\n", static_cast<unsigned long long>(hash));
}
