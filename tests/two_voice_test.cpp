#include "GranularAudio.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

int main()
{
    std::vector<int16_t> source(1009);
    for (uint32_t i = 0; i < source.size(); ++i)
    {
        source[i] = static_cast<int16_t>(25000 * std::sin(i * 0.19));
    }
    FftPitchCore audio_cores[GranularAudio::VoiceCount];
    GranularAudio audio(audio_cores);
    FftPitchCore reference[2];
    audio.update();
    for (int16_t value : AudioStream::transmitted.data)
    {
        assert(value == 0);
    }
    for (unsigned voice = 0; voice < 2; ++voice)
    {
        audio.core[voice].Attach(source.data(), source.size());
        reference[voice].Attach(source.data(), source.size());
        audio.core[voice].SetPitch(2.0f);
        reference[voice].SetPitch(2.0f);
        audio.core[voice].SetRunning(true);
        reference[voice].SetRunning(true);
    }
    audio.SetReady();
    audio.ResetMeasurement();
    for (unsigned block = 0; block < 310; ++block)
    {
        audio.update();
        for (unsigned i = 0; i < AUDIO_BLOCK_SAMPLES; ++i)
        {
            const int32_t first = reference[0].Render();
            const int32_t second = reference[1].Render();
            assert(AudioStream::transmitted.data[i] == (first + second) / static_cast<int32_t>(GranularAudio::VoiceCount));
        }
    }
    const auto first = audio.TakeTimings(0);
    const auto second = audio.TakeTimings(1);
    const auto mix = audio.TakeTimings(GranularAudio::Mix);
    assert(first.transient.blocks == 260 && second.transient.blocks == 260 && mix.transient.blocks == 260);
    assert(mix.read.blocks + mix.generate.blocks == 50);
    assert(mix.transient.engine_cycles == first.transient.engine_cycles + second.transient.engine_cycles); // Idle slots must not contribute to the reported player cost.
    assert(first.generate.hops == second.generate.hops);
    assert(mix.generate.hops == first.generate.hops + second.generate.hops);
    assert(first.generate.hops_max == 1 && second.generate.hops_max == 1 && mix.generate.hops_max == 2);
    assert(audio.TakeTimings(GranularAudio::Mix).generate.blocks == 0);
    assert(audio.TakeLevels(GranularAudio::Mix).samples == 310 * AUDIO_BLOCK_SAMPLES);
    assert(audio.TakeLevels(GranularAudio::Mix).samples == 0);
    // Stop only the first voice and sweep the second: compare the actual mixer sample by sample.
    audio.core[0].SetRunning(false);
    reference[0].SetRunning(false);
    for (unsigned block = 0; block < 40; ++block)
    {
        const float pitch = 0.5f + block * (1.5f / 39);
        audio.core[1].SetPitch(pitch);
        reference[1].SetPitch(pitch);
        audio.update();
        for (unsigned i = 0; i < AUDIO_BLOCK_SAMPLES; ++i)
        {
            const int32_t first_sample = reference[0].Render();
            const int32_t second_sample = reference[1].Render();
            assert(AudioStream::transmitted.data[i] == (first_sample + second_sample) / static_cast<int32_t>(GranularAudio::VoiceCount));
        }
    }
    assert(!audio.core[0].IsActive() && audio.core[1].IsActive());
    assert(audio.core[0].Transport() == reference[0].Transport());
    assert(audio.core[1].Transport() == reference[1].Transport());
    // A dropped output allocation must not freeze the remaining voice.
    AudioStream::fail_allocation = true;
    const auto transport = audio.core[1].Transport();
    const auto transmissions = AudioStream::transmissions;
    audio.update();
    assert(AudioStream::transmissions == transmissions);
    assert(audio.core[1].Transport() == (transport + AUDIO_BLOCK_SAMPLES) % source.size());
    uint32_t maximum, missed;
    audio.TakeDiagnostics(maximum, missed);
    assert(missed == 1);
    AudioStream::fail_allocation = false;
    audio.ResetMeasurement();
    assert(audio.TakeTimings(0).transient.blocks == 0);
    audio.update();
    assert(audio.TakeTimings(0).transient.blocks == 0);
    assert(audio.TakeTimings(1).transient.blocks == 1);
    assert(audio.TakeTimings(GranularAudio::Mix).transient.blocks == 1);
    // Full-scale coincident voices must remain in range with the fixed 1/VoiceCount mix.
    for (int16_t value : {int16_t(32767), int16_t(-32768)})
    {
        std::vector<int16_t> full_scale(1009, value);
        FftPitchCore loud_cores[GranularAudio::VoiceCount];
        GranularAudio loud(loud_cores);
        for (auto &core : loud_cores)
        {
            core.Attach(full_scale.data(), full_scale.size());
            core.SetVolume(1.0f);
            core.SetRunning(true);
        }
        loud.SetReady();
        for (unsigned block = 0; block < 40; ++block)
        {
            loud.update();
        }
        for (int16_t output : AudioStream::transmitted.data)
        {
            assert(std::abs(static_cast<int>(output) - value) < 4);
        }
    }
    FftPitchCore six_cores[GranularAudio::VoiceCount];
    FftPitchCore six_references[GranularAudio::VoiceCount];
    GranularAudio six(six_cores);
    for (unsigned voice = 0; voice < GranularAudio::VoiceCount; ++voice)
    {
        six_cores[voice].StartNote(source.data(), source.size(), 0.5f + voice * 0.5f, 0.25f);
        six_references[voice].StartNote(source.data(), source.size(), 0.5f + voice * 0.5f, 0.25f);
    }
    six.SetReady();
    for (unsigned block = 0; block < 100; ++block)
    {
        six.update();
        for (unsigned i = 0; i < AUDIO_BLOCK_SAMPLES; ++i)
        {
            int32_t sum = 0;
            for (auto &core : six_references)
            {
                sum += core.Render();
            }
            assert(AudioStream::transmitted.data[i] == sum / static_cast<int32_t>(GranularAudio::VoiceCount));
        }
    }
    std::puts("PASS: two/six FFT voices, independent pitches, exact mix, coincident frames, stop, stats, allocation failure and full-scale headroom. Host clock is mocked.");
}
