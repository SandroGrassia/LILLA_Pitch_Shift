#include "NoteVoices.h"
#include "ExperimentConfig.h"
#include <algorithm>
#include <cmath>

NoteVoices::NoteVoices(GranularAudio &audio) : audio(audio)
{
}

void NoteVoices::SetSource(Source value)
{
    source = value;
    region_first = 0;
    region_end = value.count;
    cross_mix = 0;
}

void NoteVoices::SetRegion(uint32_t first, uint32_t end, uint32_t mix)
{
    region_first = first;
    region_end = end;
    cross_mix = mix;
    ++revision;
    for (uint32_t voice = 0; voice < GranularAudio::VoiceCount; ++voice)
    {
        if (audio.core[voice].IsActive())
        {
            audio.core[voice].SetRegion(first, end, mix);
        }
    }
}

void NoteVoices::SetMode(FftPitchCore::PlayMode value)
{
    mode = value;
    ++revision;
    for (uint32_t voice = 0; voice < GranularAudio::VoiceCount; ++voice)
    {
        if (audio.core[voice].IsActive())
        {
            audio.core[voice].SetMode(value);
        }
    }
}

void NoteVoices::ReapFinished()
{
    for (uint32_t voice = 0; voice < GranularAudio::VoiceCount; ++voice)
    {
        if (notes[voice].held && !audio.core[voice].IsActive())
        {
            for (FinishedNote &entry : finished)
            {
                if (entry.order == 0)
                {
                    entry = {notes[voice].order, notes[voice].key};
                    notes[voice].held = false;
                    break;
                }
            }
            // If the ledger is full, retain ownership in this slot until NoteOff.
        }
    }
}

float NoteVoices::TargetPitch(uint8_t key) const
{
    const float bend_steps = bend * ExperimentConfig::BendSemitones / (bend < 0 ? 8192.0f : 8191.0f);
    return std::clamp(std::exp2((static_cast<int>(key) - ExperimentConfig::RootNote + bend_steps) / 12.0f), 0.25f, 4.0f);
}

NoteVoices::Result NoteVoices::NoteOn(uint8_t key, uint8_t velocity, uint32_t &voice)
{
    ReapFinished();
    // The FFT core supports 0.25..4. Reject unmapped keys instead of silently mapping them to the same pitch.
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
            note = {true, key, velocity, ++next_order, revision};
            audio.ResetVoiceMeasurement(voice);
            audio.core[voice].StartNote(source.samples, source.count, TargetPitch(key), volume * (velocity / 127.0f) / 100.0f);
            audio.core[voice].SetRegion(region_first, region_end, cross_mix);
            audio.core[voice].SetMode(mode);
            return Result::Started;
        }
    }
    ++rejected[key];
    return Result::Full;
}

bool NoteVoices::NoteOff(uint8_t key, Report &report)
{
    ReapFinished();
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
    FinishedNote *ended = nullptr;
    for (FinishedNote &entry : finished)
    {
        if (entry.order != 0 && entry.key == key && entry.order < oldest)
        {
            oldest = entry.order;
            ended = &entry;
        }
    }
    if (ended != nullptr)
    {
        ended->order = 0;
        return false;
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
    std::fill_n(finished, 128, FinishedNote{});
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

void NoteVoices::SetVolume(int volume_percent)
{
    if (volume == volume_percent)
    {
        return;
    }
    volume = volume_percent;
    ++revision;
    for (uint32_t voice = 0; voice < GranularAudio::VoiceCount; ++voice)
    {
        audio.core[voice].SetVolume(volume * (notes[voice].velocity / 127.0f) / 100.0f);
    }
}

uint32_t NoteVoices::HeldCount() const
{
    uint32_t count = 0;
    for (uint32_t voice = 0; voice < GranularAudio::VoiceCount; ++voice)
    {
        count += notes[voice].held && audio.core[voice].IsActive() ? 1u : 0u;
    }
    return count;
}

const NoteVoices::Note &NoteVoices::GetNote(uint32_t voice) const
{
    return notes[voice];
}
