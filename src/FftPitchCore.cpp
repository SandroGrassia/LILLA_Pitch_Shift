#include "FftPitchCore.h"
#include <algorithm>
#include <cmath>
#ifdef __IMXRT1062__
#include <Arduino.h>
#define FFT_SCRATCH DMAMEM
#else
#define FFT_SCRATCH
#endif

namespace
{
constexpr float Tau = 6.2831853071795864769f;
}

bool FftPitchCore::filter_ready = false;
float FftPitchCore::decimator[FftPitchCore::FilterTaps] = {};
FFT_SCRATCH float FftPitchCore::low_input[FftPitchCore::Hop * FftPitchCore::LowFactor + FftPitchCore::FilterTaps] = {};

FftPitchCore::FftPitchCore()
{
    if (!filter_ready)
    {
        float sum = 0.0f;
        for (uint32_t i = 0; i < FilterTaps; ++i)
        {
            const int offset = static_cast<int>(i) - static_cast<int>(FilterTaps / 2);
            const float sinc = offset == 0 ? 0.14f : std::sin(Tau * 0.07f * offset) / (0.5f * Tau * offset);
            const float window = 0.42f - 0.5f * std::cos(Tau * i / (FilterTaps - 1)) + 0.08f * std::cos(2.0f * Tau * i / (FilterTaps - 1));
            decimator[i] = sinc * window;
            sum += decimator[i];
        }
        for (float &value : decimator)
        {
            value /= sum;
        }
        filter_ready = true;
    }
}

void FftPitchCore::Attach(const int16_t *samples, uint32_t count)
{
    sample_reader = nullptr;
    reader_context = nullptr;
    source = samples;
    source_count = count;
    mode = requested_mode = PlayMode::LoopForward;
    cross_mix = requested_mix = 0;
    cross_mix_scale = 0.0f;
    region_first = requested_first = 0;
    region_end = requested_end = count;
    region_pending = false;
    region_gain = 1.0f;
    end_gain_now = 1.0f;
    direction_reanchor = false;
    transport = 0;
}

void FftPitchCore::StartNote(const int16_t *samples, uint32_t count, float pitch, float volume)
{
    Attach(samples, count);
    pitch_target = pitch;
    pitch_now = pitch;
    volume_target = volume;
    volume_now = volume;
    gate = 0.0f;
    initialized = false;
    transport = 0;
    high.generated = 0;
    high.analyzed = 0;
    high.transient_resets = 0;
    high.transient_holdoff = 0;
    running = true;
}

void FftPitchCore::SetRegion(uint32_t first, uint32_t end, uint32_t mix)
{
    if (high.generated == 0)
    {
        initialized = false;
    }
    requested_first = first;
    requested_end = end;
    requested_mix = mix;
    region_pending = first != region_first || end != region_end || requested_mode != mode;
    if (!initialized || !IsActive())
    {
        const uint32_t position = SourcePosition(transport);
        const bool reverse = ReadingReverse();
        mode = requested_mode;
        region_first = first;
        region_end = end;
        cross_mix = mix;
        cross_mix_scale = mix > 0 ? 1.0f / (mix - 1u) : 0.0f;
        transport = std::clamp(position, first, end - 1);
        if (IsPingPong(mode) && reverse)
        {
            transport = first + (2u * (end - first - 1u) - (transport - first)) % (2u * (end - first - 1u));
        }
        region_pending = false;
    }
}

void FftPitchCore::SetMode(PlayMode value)
{
    if (high.generated == 0)
    {
        initialized = false;
    }
    if (initialized && IsActive() && !region_pending && IsPingPong(mode) && IsPingPong(value))
    {
        mode = requested_mode = value; // Only the startup direction differs; keep the current leg and FFT history.
        return;
    }
    if (initialized && IsActive() && !region_pending && !IsLoop(mode) && !IsLoop(value))
    {
        if (mode != value)
        {
            mode = requested_mode = value;
            high.analysis_ready = false;
            direction_reanchor = true;
            low.analysis_ready = false;
            low_parts = 0;
            low_reanchor = true;
        }
        return; // Reverse at the current position without clearing overlap audio or the gate.
    }
    requested_mode = value;
    SetRegion(requested_first, requested_end, requested_mix);
    if (!initialized && IsReverse(mode))
    {
        transport = region_end - 1u;
    }
}

void FftPitchCore::SetPitch(float value) { pitch_target = value; }
void FftPitchCore::SetVolume(float value) { volume_target = value; }
void FftPitchCore::SetRunning(bool value)
{
    if (value && !IsActive())
    {
        transport = IsReverse(mode) ? region_end - 1u : region_first;
    }
    running = value;
}
bool FftPitchCore::IsActive() const { return running || gate > 0.0f; }
uint32_t FftPitchCore::Transport() const { return transport; }
uint32_t FftPitchCore::GeneratedHops() const { return high.generated; }
uint32_t FftPitchCore::AnalyzedFrames() const { return high.analyzed; }
float FftPitchCore::Pitch() const { return pitch_now; }
uint32_t FftPitchCore::TransientResets() const { return high.transient_resets; }

uint32_t FftPitchCore::SourcePosition(uint32_t position) const
{
    return IsPingPong(mode) && position >= region_end ? region_end - 1u - (position - (region_end - 1u)) : position;
}

bool FftPitchCore::ReadingReverse() const
{
    return IsPingPong(mode) ? transport >= region_end - 1u : IsReverse(mode);
}

uint32_t FftPitchCore::Advance(uint32_t position, uint32_t count) const
{
    if (IsPingPong(mode))
    {
        const uint32_t period = 2u * (region_end - region_first - 1u);
        const uint32_t next = position - region_first + count;
        return region_first + (next < period ? next : next % period);
    }
    if (mode == PlayMode::OnceReverse)
    {
        return position == UINT32_MAX || count > position - region_first ? UINT32_MAX : position - count;
    }
    if (mode == PlayMode::LoopReverse)
    {
        const uint32_t distance = position - region_first;
        if (count <= distance)
        {
            return position - count;
        }
        const uint32_t loop_end = region_end - cross_mix;
        return loop_end - 1u - (count - distance - 1u) % (loop_end - region_first);
    }
    const uint32_t next = position + count;
    if (mode == PlayMode::OnceForward)
    {
        return std::min(next, region_end);
    }
    if (next < region_end)
    {
        return next;
    }
    const uint32_t loop_start = region_first + cross_mix;
    return loop_start + (next - region_end) % (region_end - loop_start);
}

float FftPitchCore::ReadLoop(uint32_t position) const
{
    if (IsPingPong(mode))
    {
        return ReadSource(SourcePosition(position));
    }
    if (!IsLoop(mode))
    {
        return position >= region_first && position < region_end ? ReadSource(position) : 0.0f;
    }
    const float outgoing = ReadSource(position);
    if (mode == PlayMode::LoopReverse)
    {
        if (cross_mix == 0 || position >= region_first + cross_mix)
        {
            return outgoing;
        }
        const uint32_t offset = region_first + cross_mix - 1u - position;
        const float incoming = ReadSource(region_end - 1u - offset);
        const float weight = offset * cross_mix_scale;
        return outgoing + weight * (incoming - outgoing);
    }
    if (cross_mix == 0 || position < region_end - cross_mix)
    {
        return outgoing;
    }
    const uint32_t offset = position - (region_end - cross_mix);
    const float incoming = ReadSource(region_first + offset);
    const float weight = offset * cross_mix_scale;
    return outgoing + weight * (incoming - outgoing);
}

void FftPitchCore::ReadReferenceBlock(int16_t *destination, uint32_t first, uint32_t count) const
{
    // A region may have changed during the block; meters follow the new bounds.
    if (first < region_first || first >= (IsPingPong(mode) ? region_first + 2u * (region_end - region_first - 1u) : region_end))
    {
        first = mode == PlayMode::OnceReverse ? UINT32_MAX : mode == PlayMode::OnceForward && first >= region_end ? region_end : region_first;
    }
    for (uint32_t i = 0; i < count; ++i)
    {
        destination[i] = static_cast<int16_t>(ReadLoop(first));
        first = Advance(first, 1);
    }
}

void FftPitchCore::Analyze(uint32_t position)
{
    for (uint32_t i = 0; i < Size; ++i)
    {
        high.Input(i, ReadLoop(position));
        position = Advance(position, 1);
    }
    high.Analyze(direction_reanchor);
    direction_reanchor = false;
}

void FftPitchCore::Generate()
{
    if (!high.analysis_ready)
    {
        Analyze(transport);
    }
    high.Generate(pitch_now, FFT_DUAL_BAND ? FftSpectralFrame::Band::High : FftSpectralFrame::Band::Full);
}

int16_t FftPitchCore::Render()
{
    if (!IsActive())
    {
        initialized = false;
        return 0;
    }
    if (region_pending)
    {
        region_gain = std::max(0.0f, region_gain - 1.0f / 221.0f);
        if (region_gain == 0.0f)
        {
            const uint32_t position = SourcePosition(transport);
            const bool reverse = ReadingReverse();
            mode = requested_mode;
            region_first = requested_first;
            region_end = requested_end;
            cross_mix = requested_mix;
            cross_mix_scale = cross_mix > 0 ? 1.0f / (cross_mix - 1u) : 0.0f;
            transport = position;
            if (transport < region_first || transport >= region_end)
            {
                transport = IsReverse(mode) ? region_end - 1u : region_first;
            }
            if (IsPingPong(mode) && reverse)
            {
                transport = region_first + (2u * (region_end - region_first - 1u) - (transport - region_first)) % (2u * (region_end - region_first - 1u));
            }
            initialized = false;
            region_pending = false;
            high.transient_holdoff = 0;
            direction_reanchor = false;
        }
    }
    else
    {
        region_gain = std::min(1.0f, region_gain + 1.0f / Size);
    }
    if (!initialized)
    {
        InitializeRender();
    }
    pitch_now += (pitch_target - pitch_now) * (1.0f / 256.0f);
    volume_now += (volume_target - volume_now) * (1.0f / 256.0f);
    if (until_frame == 0)
    {
        if (!region_pending && cross_mix != requested_mix)
        {
            // Cross-mix-only edits keep transport, phase history and overlap audio.
            // Reanalyze this frame if lookahead was prepared using the old seam.
            cross_mix = requested_mix;
            cross_mix_scale = cross_mix > 0 ? 1.0f / (cross_mix - 1u) : 0.0f;
            if (UsesCrossMix(mode))
            {
                high.analysis_ready = false;
                low.analysis_ready = false;
            low_parts = 0;
            }
        }
        Generate();
        until_frame = Hop;
    }
    else if (until_frame == Hop / 2)
    {
        // Read-only source lookahead: analyze next frame one 128-sample block early.
        // Mapping still uses the actual pitch at synthesis time, including intervening bend.
        Analyze(Advance(transport, Hop / 2));
    }
    --until_frame;
    gate = running ? std::min(1.0f, gate + 1.0f / 221.0f) : std::max(0.0f, gate - 1.0f / 221.0f);
    // Keep FFT history warm for bend; unity output comes directly from the RAW source.
    const bool direct = pitch_target == 1.0f && std::abs(pitch_now - 1.0f) < 0.00002f;
    direct_mix = direct ? std::min(1.0f, direct_mix + 1.0f / 256.0f) : std::max(0.0f, direct_mix - 1.0f / 256.0f);
    const float direct_sample = ReadLoop(transport);
    const float processed = high.Take() + (FFT_DUAL_BAND ? RenderLow() : 0.0f);
    const float sample = direct_mix == 1.0f ? direct_sample : processed + direct_mix * (direct_sample - processed);
    const uint32_t remaining = mode == PlayMode::OnceReverse ? transport - region_first + 1u : region_end - transport;
    const uint32_t fade = std::min<uint32_t>(221u, region_end - region_first);
    const float end_gain = !IsLoop(mode) && remaining <= fade ? static_cast<float>(remaining - 1u) / (fade - 1u) : 1.0f;
    end_gain_now = std::min(end_gain, end_gain_now + 1.0f / 221.0f);
    const float value = sample * gate * volume_now * region_gain * end_gain_now;
    transport = Advance(transport, 1);
    if ((mode == PlayMode::OnceForward && transport == region_end) || (mode == PlayMode::OnceReverse && transport == UINT32_MAX))
    {
        running = false;
        initialized = false;
        gate = 0.0f;
        region_pending = false;
    }
    return static_cast<int16_t>(std::clamp(value, -32768.0f, 32767.0f));
}

float FftPitchCore::ReadOffset(uint32_t position, int offset) const
{
    if (offset >= 0)
    {
        return ReadLoop(Advance(position, static_cast<uint32_t>(offset)));
    }
    int64_t next = static_cast<int64_t>(position) + (IsReverse(mode) && !IsPingPong(mode) ? -offset : offset);
    if (!IsLoop(mode))
    {
        return next >= region_first && next < region_end ? ReadLoop(static_cast<uint32_t>(next)) : 0.0f;
    }
    if (IsPingPong(mode))
    {
        const int64_t period = 2 * static_cast<int64_t>(region_end - region_first - 1u);
        next = region_first + ((next - region_first) % period + period) % period;
    }
    else
    {
        const int64_t period = region_end - region_first - cross_mix;
        if (next < region_first)
        {
            next += ((region_first - next + period - 1) / period) * period;
        }
        else if (next >= region_end)
        {
            next -= ((next - region_end + period) / period) * period;
        }
    }
    return ReadLoop(static_cast<uint32_t>(next));
}

void FftPitchCore::FilterLowPart(uint32_t position, int offset, uint32_t part)
{
    const int first = offset + part * Hop * LowFactor;
    for (uint32_t i = 0; i < Hop * LowFactor + FilterTaps; ++i)
    {
        low_input[i] = ReadOffset(position, first + static_cast<int>(i) - static_cast<int>(FilterTaps / 2));
    }
    for (uint32_t i = 0; i < Hop; ++i)
    {
        const uint32_t base = i * LowFactor;
        float filtered = decimator[FilterTaps / 2] * low_input[base + FilterTaps / 2];
        for (uint32_t tap = 0; tap < FilterTaps / 2; ++tap)
        {
            filtered += decimator[tap] * (low_input[base + tap] + low_input[base + FilterTaps - 1u - tap]);
        }
        low_filtered[part * Hop + i] = filtered;
    }
}

void FftPitchCore::FinishLowAnalysis()
{
    for (uint32_t i = 0; i < Size; ++i)
    {
        low.Input(i, low_filtered[i]);
    }
    low.Analyze(low_reanchor);
    low_reanchor = false;
}

void FftPitchCore::AnalyzeLow(uint32_t position, int offset)
{
    // Synchronous fallback for standalone callers and mid-frame control invalidation.
    for (uint32_t part = 0; part < 4; ++part)
    {
        FilterLowPart(position, offset, part);
    }
    FinishLowAnalysis();
}

float FftPitchCore::NextLow(int offset)
{
    if (low_until_frame == 0)
    {
        if (!low.analysis_ready)
        {
            AnalyzeLow(transport, offset);
        }
        low.Generate(pitch_now, FftSpectralFrame::Band::Low);
        low_until_frame = Hop;
    }
    else if (low_until_frame <= 224 && low_until_frame >= 128 && low_until_frame % 32 == 0)
    {
        const uint32_t part = (224u - low_until_frame) / 32u;
        if (part == 0)
        {
            low_prepare_position = transport;
            low_prepare_offset = offset + LowFactor * low_until_frame;
            low_parts = 0;
        }
        if (low_parts == part)
        {
            FilterLowPart(low_prepare_position, low_prepare_offset, part);
            ++low_parts;
        }
    }
    else if (low_until_frame == 96 && low_parts == 4)
    {
        FinishLowAnalysis();
    }
    --low_until_frame;
    return low.Take();
}

float FftPitchCore::RenderLow()
{
    if (!low_primed)
    {
        low_samples[1] = NextLow(0);
        low_samples[0] = low_samples[1];
        low_samples[2] = NextLow(LowFactor);
        low_samples[3] = NextLow(2 * LowFactor);
        low_primed = true;
    }
    else if (low_phase == LowFactor)
    {
        low_samples[0] = low_samples[1];
        low_samples[1] = low_samples[2];
        low_samples[2] = low_samples[3];
        low_samples[3] = NextLow(2 * LowFactor);
        low_phase = 0;
    }
    // Cubic interpolation of the low-rate output, using file lookahead rather than output delay.
    const float t = static_cast<float>(low_phase++) / LowFactor;
    const float a = -low_samples[0] + 3.0f * low_samples[1] - 3.0f * low_samples[2] + low_samples[3];
    const float b = 2.0f * low_samples[0] - 5.0f * low_samples[1] + 4.0f * low_samples[2] - low_samples[3];
    const float c = -low_samples[0] + low_samples[2];
    return 0.5f * (((a * t + b) * t + c) * t + 2.0f * low_samples[1]);
}

void FftPitchCore::SetSampleReader(SampleReader reader, void *context)
{
    sample_reader = reader;
    reader_context = context;
}

float FftPitchCore::ReadSource(uint32_t position) const
{
    return sample_reader != nullptr ? sample_reader(reader_context, position) : source[position];
}

void FftPitchCore::FinishForwardAt(uint32_t end)
{
    mode = requested_mode = PlayMode::OnceForward;
    region_end = requested_end = end;
    cross_mix = requested_mix = 0;
    cross_mix_scale = 0.0f;
    region_pending = false;
    high.analysis_ready = false;
    low.analysis_ready = false;
    low_parts = 0;
}

void FftPitchCore::InitializeRender()
{
    high.Reset();
    low.Reset();
    low_until_frame = 0;
    low_phase = 0;
    low_primed = false;
    low_reanchor = false;
    direct_mix = pitch_target == 1.0f ? 1.0f : 0.0f;
    until_frame = 0;
    pitch_now = pitch_target;
    initialized = true;
    low_parts = 0;
    start_step = 0;
}

bool FftPitchCore::PrepareStartStep()
{
    if (!initialized)
    {
        InitializeRender();
    }
    if (FFT_DUAL_BAND && start_step < 4)
    {
        FilterLowPart(transport, 0, start_step++);
        return false;
    }
    if (FFT_DUAL_BAND && start_step == 4)
    {
        FinishLowAnalysis();
        ++start_step;
        return false;
    }
    if (!high.analysis_ready)
    {
        Analyze(transport);
        return false;
    }
    return true;
}
