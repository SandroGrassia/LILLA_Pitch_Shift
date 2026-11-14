#include "FftPitchCore.h"
#include "NoteVoices.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

using Mode = FftPitchCore::PlayMode;

int main()
{
    std::vector<int16_t> source(6000);
    for (unsigned i = 0; i < source.size(); ++i)
    {
        source[i] = static_cast<int16_t>(12000 * std::sin(i * 0.17) + 3000 * std::sin(i * 0.037));
    }
    for (unsigned first : {0u, 333u})
    {
        for (unsigned length : {2u, 100u, 1024u, 3000u})
        {
            const unsigned period = 2 * (length - 1);
            for (Mode mode : {Mode::LoopForwardReverse, Mode::LoopReverseForward})
            {
                const unsigned start = mode == Mode::LoopReverseForward ? length - 1 : 0;
                std::vector<int16_t> stream(period);
                for (unsigned i = 0; i < period; ++i)
                {
                    const unsigned phase = (i + start) % period;
                    stream[i] = source[first + (phase < length ? phase : period - phase)];
                }
                for (float pitch : {0.5f, 1.0f, 1.7f, 3.0f})
                {
                    FftPitchCore ping;
                    FftPitchCore reference;
                    ping.StartNote(source.data(), source.size(), pitch, 0.5f);
                    ping.SetRegion(first, first + length, length > 2 ? std::min(300u, length / 2u) : 0u);
                    ping.SetMode(mode);
                    reference.StartNote(stream.data(), stream.size(), pitch, 0.5f);
                    for (unsigned i = 0; i < 12000; ++i)
                    {
                        assert(ping.Transport() == first + (i + start) % period);
                        assert(ping.Render() == reference.Render());
                    }
                    const unsigned phase = (12000 + start) % period;
                    int16_t meter[128];
                    ping.ReadReferenceBlock(meter, ping.Transport(), 128);
                    for (unsigned i = 0; i < 128; ++i)
                    {
                        const unsigned at = (phase + i) % period;
                        assert(meter[i] == source[first + (at < length ? at : period - at)]);
                    }
                    // Changing the stored mix and startup direction must not affect a running ping-pong voice.
                    ping.SetRegion(first, first + length, 0);
                    ping.SetMode(mode == Mode::LoopForwardReverse ? Mode::LoopReverseForward : Mode::LoopForwardReverse);
                    for (unsigned i = 0; i < 1200; ++i)
                    {
                        assert(ping.Render() == reference.Render());
                    }
                    assert(ping.AnalyzedFrames() == reference.AnalyzedFrames());
                    assert(ping.GeneratedHops() == reference.GeneratedHops());
                    ping.SetRunning(false);
                    reference.SetRunning(false);
                    for (unsigned i = 0; i < 512; ++i)
                    {
                        assert(ping.Render() == reference.Render());
                    }
                    assert(!ping.IsActive());
                }
            }
        }
    }
    // Preserve the absolute position when leaving either ping-pong leg for a one-way loop.
    for (unsigned elapsed : {1500u, 7000u})
    {
        for (Mode target : {Mode::LoopForward, Mode::LoopReverse})
        {
            FftPitchCore ping;
            ping.StartNote(source.data(), source.size(), 1.0f, 1.0f);
            ping.SetMode(Mode::LoopForwardReverse);
            for (unsigned i = 0; i < elapsed; ++i)
            {
                ping.Render();
            }
            ping.SetMode(target);
            while (ping.Mode() != target)
            {
                const unsigned phase = ping.Transport();
                const unsigned position = phase < source.size() ? phase : 2u * (source.size() - 1u) - phase;
                ping.Render();
                if (ping.Mode() == target)
                {
                    assert(ping.Transport() == (target == Mode::LoopForward ? position + 1u : position - 1u));
                }
            }
        }
    }
    FftPitchCore cropped;
    cropped.StartNote(source.data(), source.size(), 1.7f, 0.5f);
    cropped.SetMode(Mode::LoopReverseForward);
    for (unsigned i = 0; i < 1500; ++i)
    {
        cropped.Render();
    }
    cropped.SetRegion(100, 300, 42);
    for (unsigned i = 0; i < 2000; ++i)
    {
        cropped.Render();
    }
    assert(cropped.IsActive() && cropped.Transport() >= 100 && cropped.Transport() < 498);
    auto cores = std::make_unique<FftPitchCore[]>(GranularAudio::VoiceCount);
    GranularAudio audio(cores.get());
    NoteVoices notes(audio);
    notes.SetSource({source.data(), static_cast<uint32_t>(source.size())});
    notes.SetRegion(333, 633, 42);
    notes.SetMode(Mode::LoopReverseForward);
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
    std::puts("PASS: both ping-pong modes match materialized streams, no repeated endpoints, mix ignored, live direction setting, loop transitions, bounds, meters and six voices.");
}
