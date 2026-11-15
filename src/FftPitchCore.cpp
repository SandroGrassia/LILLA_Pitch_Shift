#include "FftPitchCore.h"
#include <algorithm>
#include <cmath>
#ifdef __IMXRT1062__
#include <Arduino.h>
#include <arm_math.h>
#include <arm_const_structs.h>
#endif

namespace
{
constexpr float Tau = 6.2831853071795864769f;
}

bool FftPitchCore::tables_ready = false;
float FftPitchCore::window[FftPitchCore::Size] = {};
alignas(32) float FftPitchCore::fft[2 * FftPitchCore::Size] = {};
float FftPitchCore::advanced_phase[FftPitchCore::Bins] = {};
uint16_t FftPitchCore::peaks[FftPitchCore::Bins] = {};
uint16_t FftPitchCore::peak_owner[FftPitchCore::Bins] = {};

FftPitchCore::FftPitchCore()
{
    if (!tables_ready)
    {
        for (uint32_t i = 0; i < Size; ++i)
        {
            window[i] = 0.5f - 0.5f * std::cos(Tau * i / Size);
        }
        tables_ready = true;
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
    generated = 0;
    analyzed = 0;
    transient_resets = 0;
    transient_holdoff = 0;
    running = true;
}

void FftPitchCore::SetRegion(uint32_t first, uint32_t end, uint32_t mix)
{
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
            analysis_ready = false;
            direction_reanchor = true;
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

void FftPitchCore::FinishForwardAt(uint32_t end)
{
    mode = requested_mode = PlayMode::OnceForward;
    region_end = requested_end = end;
    cross_mix = requested_mix = 0;
    cross_mix_scale = 0.0f;
    region_pending = false;
    analysis_ready = false;
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
uint32_t FftPitchCore::GeneratedHops() const { return generated; }
uint32_t FftPitchCore::AnalyzedFrames() const { return analyzed; }
float FftPitchCore::Pitch() const { return pitch_now; }
uint32_t FftPitchCore::TransientResets() const { return transient_resets; }

float FftPitchCore::WrapPhase(float phase)
{
    return phase - Tau * std::round(phase / Tau);
}

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

void FftPitchCore::SetSampleReader(SampleReader reader, void *context)
{
    sample_reader = reader;
    reader_context = context;
}

float FftPitchCore::ReadSource(uint32_t position) const
{
    return sample_reader != nullptr ? sample_reader(reader_context, position) : source[position];
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

void FftPitchCore::Transform(bool inverse)
{
#ifdef __IMXRT1062__
    arm_cfft_f32(&arm_cfft_sR_f32_len1024, fft, inverse ? 1 : 0, 1);
#else
    // Portable host-test backend; production uses the bundled CMSIS-DSP FFT.
    for (uint32_t i = 1, j = 0; i < Size; ++i)
    {
        uint32_t bit = Size >> 1;
        while ((j & bit) != 0)
        {
            j ^= bit;
            bit >>= 1;
        }
        j ^= bit;
        if (i < j)
        {
            std::swap(fft[2 * i], fft[2 * j]);
            std::swap(fft[2 * i + 1], fft[2 * j + 1]);
        }
    }
    for (uint32_t length = 2; length <= Size; length <<= 1)
    {
        for (uint32_t j = 0; j < length / 2; ++j)
        {
            const float angle = (inverse ? Tau : -Tau) * j / length;
            const float wr = std::cos(angle);
            const float wi = std::sin(angle);
            for (uint32_t base = 0; base < Size; base += length)
            {
                const uint32_t a = 2 * (base + j);
                const uint32_t b = 2 * (base + j + length / 2);
                const float real = fft[b] * wr - fft[b + 1] * wi;
                const float imag = fft[b] * wi + fft[b + 1] * wr;
                fft[b] = fft[a] - real;
                fft[b + 1] = fft[a + 1] - imag;
                fft[a] += real;
                fft[a + 1] += imag;
            }
        }
    }
    if (inverse)
    {
        for (float &value : fft)
        {
            value /= Size;
        }
    }
#endif
}

void FftPitchCore::Analyze(uint32_t position)
{
    ++analyzed;
#ifdef __IMXRT1062__
    if (first_frame)
    {
        CrashReport.breadcrumb(3, 100);
        CrashReport.breadcrumb(4, reinterpret_cast<uint32_t>(source));
        CrashReport.breadcrumb(5, source_count);
    }
#endif
    float frame_energy = 0.0f;
    for (uint32_t i = 0; i < Size; ++i)
    {
        fft[2 * i] = ReadLoop(position) * window[i];
        frame_energy += fft[2 * i] * fft[2 * i];
        fft[2 * i + 1] = 0.0f;
        position = Advance(position, 1);
    }
#ifdef __IMXRT1062__
    if (first_frame)
    {
        CrashReport.breadcrumb(3, 101);
    }
#endif
    Transform(false);
#ifdef __IMXRT1062__
    if (first_frame)
    {
        CrashReport.breadcrumb(3, 102);
    }
#endif
    float positive_flux = 0.0f;
    float spectral_sum = 0.0f;
    for (uint32_t bin = 0; bin < Bins; ++bin)
    {
        const float real = fft[2 * bin];
        const float imag = fft[2 * bin + 1];
        const float amplitude = std::sqrt(real * real + imag * imag);
        positive_flux += std::max(0.0f, amplitude - previous_magnitude[bin]);
        spectral_sum += amplitude;
        previous_magnitude[bin] = amplitude;
        const float phase = std::atan2(imag, real);
        const float expected = Tau * bin * Hop / Size;
        const float deviation = (first_frame || direction_reanchor) ? 0.0f : WrapPhase(phase - previous_phase[bin] - expected);
        analysis_frequency[bin] = bin + deviation * (Size / (Tau * Hop));
        previous_phase[bin] = phase;
    }
    if (transient_holdoff > 0)
    {
        --transient_holdoff;
    }
    frame_transient = direction_reanchor || (!first_frame && transient_holdoff == 0 && frame_energy > Size * 64.0f * 64.0f && positive_flux > 0.35f * spectral_sum);
    direction_reanchor = false;
    if (frame_transient)
    {
        transient_holdoff = 4;
        ++transient_resets;
    }
    analysis_ready = true;
}

void FftPitchCore::Generate()
{
    ++generated;
    if (!analysis_ready)
    {
        Analyze(transport); // First frame starts immediately; no extra attack latency.
    }
    // Find peaks in the ANALYSIS spectrum: shifted bins have artificial gaps at pitch > 1.
    // Ignore peaks more than 60 dB below the largest analysis magnitude.
    const float peak_floor = *std::max_element(previous_magnitude, previous_magnitude + Bins) * 0.001f;
    uint32_t peak_count = 0;
    for (uint32_t bin = 0; bin < Bins; ++bin)
    {
        const float value = previous_magnitude[bin];
        const float left = bin > 0 ? previous_magnitude[bin - 1] : -1.0f;
        const float right = bin + 1 < Bins ? previous_magnitude[bin + 1] : -1.0f;
        if (value > peak_floor && value > left && value >= right)
        {
            peaks[peak_count++] = static_cast<uint16_t>(bin);
        }
    }
    if (peak_count > 0)
    {
        uint32_t nearest = 0;
        for (uint32_t bin = 0; bin < Bins; ++bin)
        {
            while (nearest + 1 < peak_count && 2 * bin > static_cast<uint32_t>(peaks[nearest]) + peaks[nearest + 1])
            {
                ++nearest;
            }
            peak_owner[bin] = peaks[nearest];
        }
    }
    // Shift each peak region rigidly: preserve local lobe width instead of dilating it.
    // Phase history is indexed by source bin, independent of destination collisions.
    const bool reset_phase = first_frame || frame_transient || pitch_now == 1.0f;
    for (uint32_t index = 0; index < peak_count; ++index)
    {
        const uint32_t peak = peaks[index];
        const float shift = (pitch_now - 1.0f) * analysis_frequency[peak];
        advanced_phase[peak] = reset_phase ? WrapPhase(previous_phase[peak] + 0.5f * Tau * shift) : WrapPhase(synthesis_phase[peak] + Tau * analysis_frequency[peak] * pitch_now * Hop / Size);
    }
    std::fill_n(fft, 2 * Size, 0.0f);
    for (uint32_t bin = 0; bin < Bins; ++bin)
    {
        if (peak_count == 0)
        {
            synthesis_phase[bin] = previous_phase[bin];
            continue;
        }
        const uint32_t peak = peak_owner[bin];
        const float shift = (pitch_now - 1.0f) * analysis_frequency[peak];
        const float shifted_peak = analysis_frequency[peak] * pitch_now;
        const float phase = WrapPhase(advanced_phase[peak] + previous_phase[bin] - previous_phase[peak]);
        synthesis_phase[bin] = phase;
        if (shifted_peak < 0.0f || shifted_peak > Size / 2)
        {
            continue; // Remove whole out-of-band regions; do not fold them back.
        }
        const float destination = bin + shift;
        const int lower = static_cast<int>(std::floor(destination));
        const float fraction = destination - lower;
        // Interpolate the centered complex spectrum: adjacent Hann bins have opposite signs.
        // This phase correction prevents cancellation when shifting by a fractional bin.
        const float centered_phase = phase - 0.5f * Tau * shift;
        const float real = previous_magnitude[bin] * std::cos(centered_phase);
        const float imag = previous_magnitude[bin] * std::sin(centered_phase);
        for (int side = 0; side < 2; ++side)
        {
            const int target = lower + side;
            if (target < 0 || target >= static_cast<int>(Bins))
            {
                continue;
            }
            const float weight = (side == 0 ? 1.0f - fraction : fraction) * (((target - static_cast<int>(bin)) % 2) == 0 ? 1.0f : -1.0f);
            fft[2 * target] += weight * real;
            fft[2 * target + 1] += weight * imag;
        }
    }
    fft[1] = 0.0f;
    fft[Size + 1] = 0.0f;
    for (uint32_t bin = 1; bin < Size / 2; ++bin)
    {
        fft[2 * (Size - bin)] = fft[2 * bin];
        fft[2 * (Size - bin) + 1] = -fft[2 * bin + 1];
    }
#ifdef __IMXRT1062__
    if (first_frame)
    {
        CrashReport.breadcrumb(3, 200);
    }
#endif
    Transform(true);
#ifdef __IMXRT1062__
    if (first_frame)
    {
        CrashReport.breadcrumb(3, 201);
    }
#endif
    for (uint32_t i = 0; i < Size; ++i)
    {
        accumulation[(head + i) & (Size - 1)] += fft[2 * i] * window[i] * (2.0f / 3.0f);
    }
    first_frame = false;
    analysis_ready = false;
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
            transient_holdoff = 0;
            direction_reanchor = false;
        }
    }
    else
    {
        region_gain = std::min(1.0f, region_gain + 1.0f / Size);
    }
    if (!initialized)
    {
        std::fill_n(previous_phase, Bins, 0.0f);
        std::fill_n(previous_magnitude, Bins, 0.0f);
        std::fill_n(synthesis_phase, Bins, 0.0f);
        std::fill_n(accumulation, Size, 0.0f);
        first_frame = true;
        analysis_ready = false;
        direct_mix = pitch_target == 1.0f ? 1.0f : 0.0f;
        head = 0;
        until_frame = 0;
        pitch_now = pitch_target;
        initialized = true;
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
                analysis_ready = false;
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
    const float sample = direct_mix == 1.0f ? direct_sample : accumulation[head] + direct_mix * (direct_sample - accumulation[head]);
    const uint32_t remaining = mode == PlayMode::OnceReverse ? transport - region_first + 1u : region_end - transport;
    const uint32_t fade = std::min<uint32_t>(221u, region_end - region_first);
    const float end_gain = !IsLoop(mode) && remaining <= fade ? static_cast<float>(remaining - 1u) / (fade - 1u) : 1.0f;
    end_gain_now = std::min(end_gain, end_gain_now + 1.0f / 221.0f);
    const float value = sample * gate * volume_now * region_gain * end_gain_now;
    accumulation[head] = 0.0f;
    head = (head + 1) & (Size - 1);
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
