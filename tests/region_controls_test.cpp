#include "PlaybackRegion.h"
#include "NoteVoices.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <climits>
#include <memory>
#include <vector>

int main()
{
    PlaybackRegion trim;
    trim.Reset(10000);
    assert(trim.From() == 0 && trim.To() == 9999 && trim.Step() == 625);
    trim.RotateStep(-5);
    assert(trim.Step() == 1);
    trim.MoveFrom(200);
    trim.MoveTo(-400);
    assert(trim.From() == 200 && trim.To() == 9599);
    trim.ToggleSlice();
    trim.MoveFrom(100);
    assert(trim.From() == 300 && trim.To() == 9699);
    trim.MoveFrom(INT_MAX);
    assert(trim.From() == 600 && trim.To() == 9999);
    trim.ResetFrom();
    assert(trim.From() == 0 && trim.To() == 9399);
    trim.MoveTo(INT_MIN);
    assert(trim.Length() == 100);
    trim.ResetStep();
    assert(trim.Step() == 6);
    trim.Reset(2);
    trim.MoveFrom(INT_MAX);
    trim.MoveTo(INT_MIN);
    assert(trim.From() == 0 && trim.To() == 1);
    trim.RotateStep(INT_MIN);
    assert(trim.Step() == 1);
    trim.RotateStep(INT_MAX);
    assert(trim.AutoStep());

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

    // Publish edits to six active voices and inherit them on subsequent Note Ons.
    auto cores = std::make_unique<FftPitchCore[]>(GranularAudio::VoiceCount);
    GranularAudio audio(cores.get());
    NoteVoices notes(audio);
    notes.SetSource({source.data(), static_cast<uint32_t>(source.size())});
    notes.SetRegion(300, 900);
    audio.SetReady();
    uint32_t selected;
    for (unsigned i = 0; i < GranularAudio::VoiceCount; ++i)
    {
        assert(notes.NoteOn(60 + i, 127, selected) == NoteVoices::Result::Started);
        assert(cores[i].RegionFrom() == 300 && cores[i].RegionEnd() == 900);
    }
    for (unsigned i = 0; i < 20; ++i)
    {
        audio.update();
    }
    notes.SetRegion(3000, 3100);
    for (unsigned i = 0; i < 20; ++i)
    {
        audio.update();
    }
    for (unsigned i = 0; i < GranularAudio::VoiceCount; ++i)
    {
        assert(cores[i].RegionFrom() == 3000 && cores[i].RegionEnd() == 3100);
        assert(cores[i].Transport() >= 3000 && cores[i].Transport() < 3100);
        NoteVoices::Report report;
        assert(notes.NoteOff(60 + i, report));
        assert(report.parameters_changed);
    }
    for (unsigned i = 0; i < 4; ++i)
    {
        audio.update();
    }
    assert(notes.NoteOn(72, 100, selected) == NoteVoices::Result::Started);
    assert(cores[selected].RegionFrom() == 3000);
    std::puts("PASS: Lilla trim controls, crop equivalence at 0.5..3, tiny loops, live edits, release during edits and six-voice region publication.");
}
