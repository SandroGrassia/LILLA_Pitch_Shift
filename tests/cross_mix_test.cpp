#include "FftPitchCore.h"
#include "PlaybackRegion.h"
#include "NoteVoices.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

int main()
{
    PlaybackRegion region;
    region.Reset(1000);
    region.RotateStep(-5);
    region.MoveCrossMix(1);
    assert(region.CrossMix() == 2);
    region.MoveCrossMix(-1);
    assert(region.CrossMix() == 0);
    region.MoveCrossMix(10000);
    assert(region.CrossMix() == 300);
    region.MoveTo(-900);
    assert(region.Length() == 100 && region.CrossMix() == 42);
    region.MoveTo(900);
    assert(region.CrossMix() == 42);
    region.Reset(674);
    assert(region.CrossMixMax() == 42);
    region.Reset(675);
    assert(region.CrossMixMax() == 300);
    region.MoveCrossMix(15);
    assert(region.CrossMix() == 30);
    region.MoveCrossMix(1);
    assert(region.CrossMix() == 34);
    region.MoveCrossMix(14);
    assert(region.CrossMix() == 90);
    region.MoveCrossMix(1);
    assert(region.CrossMix() == 100);
    region.MoveCrossMix(-1);
    assert(region.CrossMix() == 90);
    region.MoveCrossMix(-15);
    assert(region.CrossMix() == 30);
    region.RotateStep(5);
    region.MoveCrossMix(1);
    assert(region.CrossMix() == 34); // Independent of the trim increment.
    region.Reset(2);
    region.MoveCrossMix(1);
    assert(region.CrossMix() == 0);

    std::vector<int16_t> source(1400);
    for (unsigned i = 0; i < source.size(); ++i)
    {
        source[i] = static_cast<int16_t>(10000 * std::sin(i * 0.073));
    }
    for (unsigned length : {4u, 100u, 1000u})
    {
        for (unsigned mix : {0u, 2u, length / 2})
        {
            FftPitchCore core;
            core.StartNote(source.data(), source.size(), 1.0f, 1.0f);
            core.SetRegion(100, 100 + length, mix);
            unsigned position = 100;
            for (unsigned i = 0; i < 6000; ++i)
            {
                assert(core.Transport() == position);
                float expected = source[position];
                if (mix > 0 && position >= 100 + length - mix)
                {
                    const unsigned offset = position - (100 + length - mix);
                    expected += (static_cast<float>(offset) / (mix - 1)) * (source[100 + offset] - expected);
                }
                const int actual = core.Render();
                if (i > 221)
                {
                    assert(std::abs(actual - expected) < 2.0f);
                }
                if (++position == 100 + length)
                {
                    position = 100 + mix;
                }
            }
        }
    }
    // A two-sample mix has exact integer weights. Compare FFT input against an
    // independently materialized stream including first traversal and repeated skips.
    std::vector<int16_t> materialized(24000);
    unsigned position = 100;
    for (auto &sample : materialized)
    {
        sample = position == 199 ? source[101] : source[position];
        if (++position == 200)
        {
            position = 102;
        }
    }
    for (float pitch : {0.5f, 1.0f, 1.7f, 3.0f})
    {
        FftPitchCore mixed;
        FftPitchCore reference;
        mixed.StartNote(source.data(), source.size(), pitch, 0.5f);
        mixed.SetRegion(100, 200, 2);
        reference.StartNote(materialized.data(), materialized.size(), pitch, 0.5f);
        for (unsigned i = 0; i < 12000; ++i)
        {
            assert(mixed.Render() == reference.Render());
        }
    }
    // At unity the new wrap continues from the head already mixed into the tail.
    std::vector<int16_t> discontinuous(1000, 12000);
    std::fill(discontinuous.begin() + 500, discontinuous.end(), -12000);
    FftPitchCore seam;
    seam.StartNote(discontinuous.data(), discontinuous.size(), 1.0f, 1.0f);
    seam.SetRegion(0, 1000, 100);
    int previous = 0;
    int maximum = 0;
    for (unsigned i = 0; i < 1002; ++i)
    {
        const int sample = seam.Render();
        if (i >= 900)
        {
            maximum = std::max(maximum, std::abs(sample - previous));
        }
        previous = sample;
    }
    assert(maximum < 245); // Original seam jumps by 24000 PCM units.

    // Mix-only edits must not fade, restart playback, or reset the FFT schedule.
    std::vector<int16_t> dc(5000, 12000);
    for (float pitch : {1.0f, 1.7f})
    {
        FftPitchCore continuous;
        continuous.StartNote(dc.data(), dc.size(), pitch, 1.0f);
        for (unsigned i = 0; i < 2048; ++i)
        {
            continuous.Render();
        }
        unsigned expected_position = continuous.Transport();
        unsigned expected_hops = continuous.GeneratedHops();
        for (unsigned i = 0; i < 8192; ++i)
        {
            if (i % 73 == 0)
            {
                continuous.SetRegion(0, dc.size(), (i / 73) % 2 ? 300 : 0);
            }
            assert(continuous.Transport() == expected_position);
            const int sample = continuous.Render();
            assert(std::abs(sample - 12000) <= 2); // No attenuation during repeated edits.
            if (i % FftPitchCore::Hop == 0)
            {
                ++expected_hops;
            }
            assert(continuous.GeneratedHops() == expected_hops);
            if (++expected_position == dc.size())
            {
                expected_position = continuous.CrossMix();
            }
        }
        continuous.SetRegion(0, dc.size(), 0);
        continuous.SetRunning(false);
        for (unsigned i = 0; i < 500; ++i)
        {
            continuous.Render();
        }
        assert(!continuous.IsActive());
    }
    // ONCE ignores cross-mix completely, including edits made while rendering.
    FftPitchCore once_edit;
    FftPitchCore once_reference;
    once_edit.StartNote(source.data(), source.size(), 1.7f, 1.0f);
    once_reference.StartNote(source.data(), source.size(), 1.7f, 1.0f);
    once_edit.SetMode(FftPitchCore::PlayMode::OnceForward);
    once_reference.SetMode(FftPitchCore::PlayMode::OnceForward);
    for (unsigned i = 0; i < source.size() + 300; ++i)
    {
        if (i == 400)
        {
            once_edit.SetRegion(0, source.size(), 300);
        }
        assert(once_edit.Render() == once_reference.Render());
    }

    auto cores = std::make_unique<FftPitchCore[]>(GranularAudio::VoiceCount);
    GranularAudio audio(cores.get());
    NoteVoices notes(audio);
    notes.SetSource({source.data(), static_cast<uint32_t>(source.size())});
    notes.SetRegion(100, 1100, 100);
    audio.SetReady();
    uint32_t selected;
    for (unsigned i = 0; i < GranularAudio::VoiceCount; ++i)
    {
        assert(notes.NoteOn(60 + i, 127, selected) == NoteVoices::Result::Started);
        assert(cores[i].CrossMix() == 100);
    }
    for (unsigned i = 0; i < 10; ++i)
    {
        audio.update();
    }
    notes.SetRegion(300, 400, 50);
    for (unsigned i = 0; i < 20; ++i)
    {
        audio.update();
    }
    for (unsigned i = 0; i < GranularAudio::VoiceCount; ++i)
    {
        assert(cores[i].CrossMix() == 50);
        assert(cores[i].Transport() >= 350 && cores[i].Transport() < 400);
    }
    notes.SetRegion(300, 400, 0);
    for (unsigned i = 0; i < 20; ++i)
    {
        audio.update();
    }
    for (unsigned i = 0; i < GranularAudio::VoiceCount; ++i)
    {
        assert(cores[i].CrossMix() == 0);
    }
    std::puts("PASS: cross-mix control bounds, RAW blend, loop timing, FFT materialized-stream equivalence, seam reduction, live edits and six voices.");
}
