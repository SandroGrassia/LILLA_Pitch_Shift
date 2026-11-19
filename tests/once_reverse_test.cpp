#include "FftPitchCore.h"
#include "NoteVoices.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

int main()
{
    std::vector<int16_t> source(5000);
    for (unsigned i = 0; i < source.size(); ++i)
    {
        source[i] = static_cast<int16_t>(12000 * std::sin(i * 0.17));
    }
    for (unsigned start : {0u, 333u})
    {
        for (unsigned length : {2u, 100u, 1024u, 3000u})
        {
            std::vector<int16_t> reversed(length);
            for (unsigned i = 0; i < length; ++i)
            {
                reversed[i] = source[start + length - 1 - i];
            }
            for (float pitch : {0.5f, 1.0f, 1.7f, 3.0f})
            {
                FftPitchCore backward;
                FftPitchCore reference;
                backward.StartNote(source.data(), source.size(), pitch, 0.5f);
                backward.SetRegion(start, start + length, length > 2 ? 2 : 0);
                backward.SetMode(FftPitchCore::PlayMode::OnceReverse);
                reference.StartNote(reversed.data(), reversed.size(), pitch, 0.5f);
                reference.SetMode(FftPitchCore::PlayMode::OnceForward);
                for (unsigned i = 0; i < length; ++i)
                {
                    assert(backward.IsActive());
                    assert(backward.Transport() == start + length - 1 - i);
                    assert(backward.Render() == reference.Render());
                }
                assert(!backward.IsActive() && backward.Transport() == UINT32_MAX);
                int16_t meter[128];
                backward.ReadReferenceBlock(meter, UINT32_MAX, 128);
                for (int16_t sample : meter)
                {
                    assert(sample == 0);
                }
                for (unsigned i = 0; i < 512; ++i)
                {
                    assert(backward.Render() == 0);
                }
            }
        }
    }
    FftPitchCore live;
    live.StartNote(source.data(), source.size(), 1.7f, 1.0f);
    for (unsigned i = 0; i < 2000; ++i)
    {
        live.Render();
    }
    live.SetMode(FftPitchCore::PlayMode::OnceReverse);
    for (unsigned i = 0; i < 500; ++i)
    {
        live.Render();
    }
    assert(live.IsActive() && live.Mode() == FftPitchCore::PlayMode::OnceReverse);
    const unsigned before = live.Transport();
    live.Render();
    assert(live.Transport() == before - 1);
    live.SetRegion(3000, 4000);
    for (unsigned i = 0; i < 400; ++i)
    {
        live.Render();
    }
    assert(live.Transport() >= 3000 && live.Transport() < 4000);
    live.SetRunning(false);
    for (unsigned i = 0; i < 500; ++i)
    {
        live.Render();
    }
    assert(!live.IsActive());

    // Reverse at different points of the split analysis/synthesis schedule.
    for (float pitch : {1.0f, 1.7f, 3.0f})
    {
        for (unsigned offset : {0u, 1u, 127u, 128u, 255u})
        {
            std::vector<int16_t> constant(12000, 10000);
            FftPitchCore changing;
            changing.StartNote(constant.data(), constant.size(), pitch, 1.0f);
            changing.SetMode(FftPitchCore::PlayMode::OnceForward);
            for (unsigned i = 0; i < 4096 + offset; ++i)
            {
                changing.Render();
            }
            for (auto direction : {FftPitchCore::PlayMode::OnceReverse, FftPitchCore::PlayMode::OnceForward})
            {
                const unsigned position = changing.Transport();
                const unsigned hops = changing.GeneratedHops();
                changing.SetMode(direction);
                assert(changing.Transport() == position);
                for (unsigned i = 0; i < 1024; ++i)
                {
                    const int sample = changing.Render();
                    assert(std::abs(sample - 10000) < 100);
                    assert(changing.Transport() == (direction == FftPitchCore::PlayMode::OnceReverse ? position - i - 1 : position + i + 1));
                }
                assert(changing.GeneratedHops() == hops + 4);
            }
        }
    }
    // Unity reads exactly the current RAW sample on each side of a reversal.
    FftPitchCore transparent;
    transparent.StartNote(source.data(), source.size(), 1.0f, 1.0f);
    transparent.SetMode(FftPitchCore::PlayMode::OnceForward);
    for (unsigned i = 0; i < 2048; ++i)
    {
        transparent.Render();
    }
    for (auto direction : {FftPitchCore::PlayMode::OnceReverse, FftPitchCore::PlayMode::OnceForward})
    {
        transparent.SetMode(direction);
        for (unsigned i = 0; i < 512; ++i)
        {
            const unsigned position = transparent.Transport();
            assert(transparent.Render() == source[position]);
        }
    }
    // Reversing away from the terminal fade restores gain smoothly, without restart.
    std::vector<int16_t> constant(5000, 10000);
    FftPitchCore fading;
    fading.StartNote(constant.data(), constant.size(), 1.0f, 1.0f);
    fading.SetMode(FftPitchCore::PlayMode::OnceForward);
    int previous = 0;
    for (unsigned i = 0; i < 4950; ++i)
    {
        previous = fading.Render();
    }
    fading.SetMode(FftPitchCore::PlayMode::OnceReverse);
    for (unsigned i = 0; i < 256; ++i)
    {
        const int sample = fading.Render();
        assert(sample >= previous && sample - previous <= 47);
        previous = sample;
    }
    assert(previous == 10000);

    auto cores = std::make_unique<FftPitchCore[]>(GranularAudio::VoiceCount);
    GranularAudio audio(cores.get());
    NoteVoices notes(audio);
    notes.SetSource({source.data(), static_cast<uint32_t>(source.size())});
    notes.SetRegion(0, 100);
    notes.SetMode(FftPitchCore::PlayMode::OnceReverse);
    audio.SetReady();
    uint32_t selected;
    for (unsigned i = 0; i < GranularAudio::VoiceCount; ++i)
    {
        assert(notes.NoteOn(60 + i, 100, selected) == NoteVoices::Result::Started);
        assert(cores[i].Transport() == 99);
    }
    audio.update();
    assert(notes.HeldCount() == 0);
    assert(notes.NoteOn(60, 100, selected) == NoteVoices::Result::Started);
    NoteVoices::Report report;
    assert(!notes.NoteOff(60, report));
    assert(cores[selected].IsActive());
    assert(notes.NoteOff(60, report));
    std::puts("PASS: ONCE_REV matches reversed-source ONCE_FWD exactly, start/end bounds, lookahead silence, pitch/duration, live edits and configured-voice reuse.");
}
