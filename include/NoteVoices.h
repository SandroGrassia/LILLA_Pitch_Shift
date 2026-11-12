#pragma once
#include "GranularAudio.h"

// DSP mutations require the audio IRQ disabled (or playback not yet enabled).
// Metadata belongs to the foreground. No allocation, logging or file I/O here.
class NoteVoices
{
public:
    struct Source
    {
        const int16_t *samples = nullptr;
        uint32_t count = 0;
    };
    enum class Result { Started, Full, OutOfRange, WaitingNoteOff };
    struct Note
    {
        bool held = false;
        uint8_t key = 0;
        uint8_t velocity = 0;
        uint8_t source = 0;
        uint64_t order = 0;
        uint32_t revision = 0;
    };
    struct Report
    {
        uint32_t voice = 0;
        Note note;
        GranularAudio::Timings timings;
        bool parameters_changed = false;
    };
    explicit NoteVoices(GranularAudio &audio);
    void SetSources(Source first, Source second);
    Result NoteOn(uint8_t key, uint8_t velocity, uint32_t &voice);
    bool NoteOff(uint8_t key, Report &report); // Repeated equal notes are released oldest first.
    bool ReleaseVoice(uint32_t voice, Report &report);
    void ClearRejected();
    void PitchBend(int value); // MIDI signed range -8192..8191.
    void SetAntialias(bool enabled);
    void SetParameters(uint32_t window, uint32_t overlap, int volume_percent);
    uint32_t HeldCount() const;
    const Note &GetNote(uint32_t voice) const;
    float TargetPitch(uint8_t key) const;

private:
    GranularAudio &audio;
    Source sources[2];
    Note notes[GranularAudio::VoiceCount];
    uint32_t rejected[128] = {}; // Preserve FIFO NoteOff ownership after a capacity rejection.
    uint64_t next_order = 0;
    uint8_t next_source = 0;
    int bend = 0;
    bool antialias_enabled = true;
    int volume = 25;
    uint32_t window = 882;
    uint32_t overlap = 50;
    uint32_t revision = 0;
};
