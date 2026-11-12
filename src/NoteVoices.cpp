#include "NoteVoices.h"
#include "ExperimentConfig.h"
#include <algorithm>
#include <cmath>

NoteVoices::NoteVoices(GranularAudio &audio) : audio(audio)
{
}

void NoteVoices::SetSources(Source first, Source second)
{
    sources[0] = first;
    sources[1] = second;
}

float NoteVoices::TargetPitch(uint8_t key) const
{
    const float bend_steps = bend * ExperimentConfig::BendSemitones / (bend < 0 ? 8192.0f : 8191.0f);
    return std::clamp(std::exp2((static_cast<int>(key) - ExperimentConfig::RootNote + bend_steps) / 12.0f), 0.25f, 4.0f);
}

NoteVoices::Result NoteVoices::NoteOn(uint8_t key, uint8_t velocity, uint32_t &voice)
{
    // The WSOLA core supports 0.25..4. Reject unmapped keys instead of silently mapping them to the same pitch.
    if (key < ExperimentConfig::RootNote - 24 || key > ExperimentConfig::RootNote + 24)
    {
        ++rejected[key];
        return Result::OutOfRange;
    }
    if (rejected[key] > 0)
    {
        ++rejected[key];
        return Result::WaitingNoteOff;
    }
    for (voice = 0; voice < GranularAudio::VoiceCount; ++voice)
    {
        if (!notes[voice].held && !audio.core[voice].IsActive())
        {
            Note &note = notes[voice];
            note = {true, key, velocity, next_source, ++next_order, revision};
            const Source &source = sources[next_source];
            next_source ^= 1u;
            audio.ResetVoiceMeasurement(voice);
            audio.core[voice].SetWindow(window);
            audio.core[voice].SetOverlap(overlap);
            audio.core[voice].StartNote(source.samples, source.count, TargetPitch(key), volume * (velocity / 127.0f) / 100.0f);
            return Result::Started;
        }
    }
    ++rejected[key];
    return Result::Full;
}

bool NoteVoices::NoteOff(uint8_t key, Report &report)
{
    uint32_t selected = GranularAudio::VoiceCount;
    uint64_t oldest = UINT64_MAX;
    for (uint32_t voice = 0; voice < GranularAudio::VoiceCount; ++voice)
    {
        if (notes[voice].held && notes[voice].key == key && notes[voice].order < oldest)
        {
            oldest = notes[voice].order;
            selected = voice;
        }
    }
    if (selected != GranularAudio::VoiceCount)
    {
        return ReleaseVoice(selected, report);
    }
    if (rejected[key] > 0)
    {
        --rejected[key];
    }
    return false;
}

bool NoteVoices::ReleaseVoice(uint32_t voice, Report &report)
{
    if (!notes[voice].held)
    {
        return false;
    }
    report.voice = voice;
    report.note = notes[voice];
    report.parameters_changed = notes[voice].revision != revision;
    report.timings = audio.TakeTimings(voice);
    notes[voice].held = false;
    audio.core[voice].SetRunning(false);
    return true;
}

void NoteVoices::ClearRejected()
{
    std::fill_n(rejected, 128, 0u);
}

void NoteVoices::PitchBend(int value)
{
    if (value == bend)
    {
        return;
    }
    bend = value;
    ++revision;
    for (uint32_t voice = 0; voice < GranularAudio::VoiceCount; ++voice)
    {
        if (audio.core[voice].IsActive())
        {
            audio.core[voice].SetPitch(TargetPitch(notes[voice].key));
        }
    }
}

void NoteVoices::SetParameters(uint32_t next_window, uint32_t next_overlap, int volume_percent)
{
    if (window == next_window && overlap == next_overlap && volume == volume_percent)
    {
        return;
    }
    window = next_window;
    overlap = next_overlap;
    volume = volume_percent;
    ++revision;
    for (uint32_t voice = 0; voice < GranularAudio::VoiceCount; ++voice)
    {
        audio.core[voice].SetWindow(window);
        audio.core[voice].SetOverlap(overlap);
        audio.core[voice].SetVolume(volume * (notes[voice].velocity / 127.0f) / 100.0f);
    }
}

void NoteVoices::SetAntialias(bool enabled)
{
    if (antialias_enabled == enabled)
    {
        return;
    }
    antialias_enabled = enabled;
    ++revision;
    for (uint32_t voice = 0; voice < GranularAudio::VoiceCount; ++voice)
    {
        audio.core[voice].SetAntialias(enabled);
    }
}

uint32_t NoteVoices::HeldCount() const
{
    uint32_t count = 0;
    for (const Note &note : notes)
    {
        count += note.held ? 1u : 0u;
    }
    return count;
}

const NoteVoices::Note &NoteVoices::GetNote(uint32_t voice) const
{
    return notes[voice];
}
