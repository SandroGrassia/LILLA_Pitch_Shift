#include "FftPitchCore.h"
#include "NoteVoices.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

int main()
{
    using Mode = FftPitchCore::PlayMode;
    std::vector<int16_t> source(6000);
    for (unsigned i = 0; i < source.size(); ++i)
    {
        source[i] = static_cast<int16_t>(12000 * std::sin(i * 0.17) + 3000 * std::sin(i * 0.037));
    }
    for (unsigned first : {0u, 333u})
    {
        for (unsigned length : {2u, 100u, 1024u, 3000u})
        {
            std::vector<int16_t> reversed(length);
            for (unsigned i = 0; i < length; ++i)
            {
                reversed[i] = source[first + length - 1u - i];
            }
            for (unsigned mix : {0u, length > 2 ? 2u : 0u, std::min(300u, length / 2u)})
            {
                if (mix == 1)
                {
                    continue;
                }
                for (float pitch : {0.5f, 1.0f, 1.7f, 3.0f})
                {
                    FftPitchCore backward;
                    FftPitchCore forward;
                    backward.StartNote(source.data(), source.size(), pitch, 0.5f);
                    backward.SetRegion(first, first + length, mix);
                    backward.SetMode(Mode::LoopReverse);
                    forward.StartNote(reversed.data(), reversed.size(), pitch, 0.5f);
                    forward.SetRegion(0, length, mix);
                    assert(backward.Transport() == first + length - 1u);
                    for (unsigned i = 0; i < 12000; ++i)
                    {
                        assert(backward.Transport() == first + length - 1u - forward.Transport());
                        assert(backward.Render() == forward.Render());
                    }
                    assert(backward.IsActive());
                    // Changing XMIX while looping keeps the same uninterrupted reversed stream.
                    backward.SetRegion(first, first + length, 0);
                    forward.SetRegion(0, length, 0);
                    for (unsigned i = 0; i < 1500; ++i)
                    {
                        assert(backward.Render() == forward.Render());
                    }
                    backward.SetRunning(false);
                    forward.SetRunning(false);
                    for (unsigned i = 0; i < 512; ++i)
                    {
                        assert(backward.Render() == forward.Render());
                    }
                    assert(!backward.IsActive());
                }
            }
        }
    }
    FftPitchCore editing;
    editing.StartNote(source.data(), source.size(), 1.7f, 0.5f);
    editing.SetMode(Mode::LoopReverse);
    for (unsigned i = 0; i < 1500; ++i)
    {
        editing.Render();
    }
    editing.SetRegion(100, 300, 42);
    for (unsigned i = 0; i < 2000; ++i)
    {
        editing.Render();
    }
    assert(editing.IsActive() && editing.Transport() >= 100 && editing.Transport() < 300);
    auto cores = std::make_unique<FftPitchCore[]>(GranularAudio::VoiceCount);
    GranularAudio audio(cores.get());
    NoteVoices notes(audio);
    notes.SetSource({source.data(), static_cast<uint32_t>(source.size())});
    notes.SetRegion(333, 633, 42);
    notes.SetMode(Mode::LoopReverse);
    audio.SetReady();
    uint32_t selected;
    for (unsigned i = 0; i < GranularAudio::VoiceCount; ++i)
    {
        assert(notes.NoteOn(60 + i, 100, selected) == NoteVoices::Result::Started);
        assert(cores[selected].Transport() == 632);
    }
    for (unsigned i = 0; i < 80; ++i)
    {
        audio.update();
    }
    assert(notes.HeldCount() == GranularAudio::VoiceCount);
    NoteVoices::Report report;
    for (unsigned i = 0; i < GranularAudio::VoiceCount; ++i)
    {
        assert(notes.NoteOff(60 + i, report));
    }
    for (unsigned i = 0; i < 4; ++i)
    {
        audio.update();
    }
    for (unsigned i = 0; i < GranularAudio::VoiceCount; ++i)
    {
        assert(!cores[i].IsActive());
    }
    std::puts("PASS: LOOP_REV equals reversed-source LOOP_FWD, tiny regions, repeated seams, cross-mix edits, pitch 0.5..3, live bounds, release and configured voices.");
}
