#include "FftPitchCore.h"
#include "NoteVoices.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>
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

    auto cores = std::make_unique<FftPitchCore[]>(GranularAudio::VoiceCount);
    GranularAudio audio(cores.get());
    NoteVoices notes(audio);
    notes.SetSource({source.data(), static_cast<uint32_t>(source.size())});
    notes.SetRegion(300, 400);
    notes.SetMode(FftPitchCore::PlayMode::OnceForward);
    audio.SetReady();
    uint32_t selected;
    for (unsigned i = 0; i < GranularAudio::VoiceCount; ++i)
    {
        assert(notes.NoteOn(60 + i, 100, selected) == NoteVoices::Result::Started);
    }
    audio.update();
    assert(notes.HeldCount() == 0);
    // Freed slots can play again before the keyboard releases the previous notes.
    assert(notes.NoteOn(60, 100, selected) == NoteVoices::Result::Started);
    NoteVoices::Report report;
    assert(!notes.NoteOff(60, report)); // Old finished occurrence.
    assert(cores[selected].IsActive());
    assert(notes.NoteOff(60, report)); // New assigned occurrence.
    assert(!cores[selected].IsActive()); // Released before rendering its first block.
    for (unsigned i = 1; i < GranularAudio::VoiceCount; ++i)
    {
        assert(!notes.NoteOff(60 + i, report));
    }
    assert(notes.NoteOn(60, 100, selected) == NoteVoices::Result::Started);
    audio.update();
    assert(!cores[selected].IsActive());
    notes.SetMode(FftPitchCore::PlayMode::LoopForward);
    audio.update();
    assert(!cores[selected].IsActive()); // Mode edits do not resurrect ended voices.
    notes.ClearRejected();
    notes.SetRegion(300, 2300);
    notes.SetMode(FftPitchCore::PlayMode::OnceForward);
    assert(notes.NoteOn(64, 100, selected) == NoteVoices::Result::Started);
    const unsigned older = selected;
    assert(notes.NoteOn(64, 100, selected) == NoteVoices::Result::Started);
    cores[selected].SetRegion(300, 400); // Newer occurrence finishes before the older one.
    audio.update();
    assert(!cores[selected].IsActive() && cores[older].IsActive());
    assert(notes.NoteOff(64, report));
    assert(report.voice == older); // FIFO follows NoteOn order, not completion order.
    assert(!notes.NoteOff(64, report));
    std::puts("PASS: ONCE duration at all pitches, zero-padded FFT reads, no cross-mix, mode changes, NoteOff, natural-end capacity and delayed NoteOff ownership.");
}
