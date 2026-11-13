#include "NoteVoices.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

int main()
{
    auto cores = std::make_unique<FftPitchCore[]>(GranularAudio::VoiceCount);
    GranularAudio audio(cores.get());
    NoteVoices voices(audio);
    std::vector<int16_t> first(1009, 12000);
    voices.SetSource({first.data(), static_cast<uint32_t>(first.size())});
    audio.SetReady();
    uint32_t selected;
    for (uint32_t i = 0; i < GranularAudio::VoiceCount; ++i)
    {
        assert(voices.NoteOn(60, 127, selected) == NoteVoices::Result::Started);
        assert(selected == i);
    }
    assert(voices.HeldCount() == GranularAudio::VoiceCount);
    assert(voices.NoteOn(60, 127, selected) == NoteVoices::Result::Full);
    for (uint32_t block = 0; block < 300; ++block)
    {
        audio.update();
    }
    // Six voices sharing one source, each at 25%, fixed gain 1/6.
    for (int16_t sample : AudioStream::transmitted.data)
    {
        assert(std::abs(sample - 3000) <= 2);
    }
    assert(audio.core[0].Transport() == (300u * AUDIO_BLOCK_SAMPLES) % first.size());
    assert(audio.core[1].Transport() == (300u * AUDIO_BLOCK_SAMPLES) % first.size());
    // Reading MIX diagnostics must not clear the per-note statistics.
    audio.TakeTimings(GranularAudio::Mix);
    NoteVoices::Report report;
    assert(voices.NoteOff(60, report));
    assert(report.voice == 0 && !report.parameters_changed);
    assert(report.timings.transient.blocks == 260);
    assert(report.timings.read.blocks + report.timings.generate.blocks == 40);
    assert(voices.HeldCount() == GranularAudio::VoiceCount - 1 && audio.core[0].IsActive());
    assert(voices.NoteOn(61, 127, selected) == NoteVoices::Result::Full);
    // A rejected retrigger may not acquire a freed voice before its older NoteOff arrives.
    for (uint32_t i = 0; i < 3; ++i)
    {
        audio.update();
    }
    assert(!audio.core[0].IsActive());
    assert(voices.NoteOn(60, 127, selected) == NoteVoices::Result::WaitingNoteOff);
    assert(!voices.NoteOff(61, report));
    assert(voices.NoteOn(61, 64, selected) == NoteVoices::Result::Started);
    assert(selected == 0);
    audio.update();
    assert(audio.core[0].Transport() == AUDIO_BLOCK_SAMPLES);
    assert(std::fabs(audio.core[0].Pitch() - std::exp2(1.0f / 12.0f)) < 0.00001f);
    assert(voices.NoteOff(61, report));
    assert(report.timings.transient.blocks == 1 && report.timings.read.blocks == 0 && report.timings.generate.blocks == 0);
    // Each equal-note NoteOff releases only the oldest still-held instance.
    for (uint32_t i = 1; i < GranularAudio::VoiceCount; ++i)
    {
        assert(voices.NoteOff(60, report));
        assert(report.voice == i);
    }
    assert(!voices.NoteOff(60, report));
    assert(!voices.NoteOff(60, report));
    assert(voices.HeldCount() == 0);
    for (uint32_t i = 0; i < 3; ++i)
    {
        audio.update();
    }
    assert(voices.NoteOn(60, 127, selected) == NoteVoices::Result::Started);
    voices.PitchBend(8191);
    assert(std::fabs(voices.TargetPitch(60) - std::exp2(2.0f / 12.0f)) < 0.00001f);
    assert(voices.TargetPitch(84) == 4.0f);
    voices.PitchBend(-8192);
    assert(std::fabs(voices.TargetPitch(60) - std::exp2(-2.0f / 12.0f)) < 0.00001f);
    assert(voices.TargetPitch(36) == 0.25f);
    voices.PitchBend(0);
    voices.SetVolume(100);
    audio.update();
    assert(voices.NoteOff(60, report));
    assert(report.parameters_changed && report.timings.transient.blocks == 1);
    assert(voices.NoteOn(35, 127, selected) == NoteVoices::Result::OutOfRange);
    assert(voices.NoteOn(85, 127, selected) == NoteVoices::Result::OutOfRange);
    assert(!voices.NoteOff(35, report));
    assert(!voices.NoteOff(85, report));
    // MIDI bursts can start and end a note before the next audio callback.
    assert(voices.NoteOn(72, 100, selected) == NoteVoices::Result::Started);
    voices.SetVolume(50);
    assert(voices.NoteOff(72, report));
    assert(report.parameters_changed);
    assert(report.timings.transient.blocks == 0);
    assert(!audio.core[report.voice].IsActive());
    assert(voices.NoteOn(48, 100, selected) == NoteVoices::Result::Started);
    assert(voices.NoteOn(49, 100, selected) == NoteVoices::Result::Started);
    for (uint32_t i = 0; i < GranularAudio::VoiceCount; ++i)
    {
        voices.ReleaseVoice(i, report);
    }
    voices.ClearRejected();
    assert(voices.HeldCount() == 0);
    std::puts("PASS: 6 FFT voices sharing a source, FIFO repeated notes, capacity/rejected ownership, tails, restart, per-note measurements, pitch bend, volume and all-notes-off. Host timing is mocked.");
}
