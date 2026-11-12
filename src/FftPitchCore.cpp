#include "FftPitchCore.h"
#include <algorithm>
#include <cmath>
#ifdef __IMXRT1062__
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
float FftPitchCore::magnitude[FftPitchCore::Bins] = {};
float FftPitchCore::frequency_sum[FftPitchCore::Bins] = {};
float FftPitchCore::strongest[FftPitchCore::Bins] = {};
float FftPitchCore::initial_phase[FftPitchCore::Bins] = {};
float FftPitchCore::advanced_phase[FftPitchCore::Bins] = {};
uint16_t FftPitchCore::dominant_source[FftPitchCore::Bins] = {};
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
    source = samples;
    source_count = count;
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
    transient_resets = 0;
    transient_holdoff = 0;
    running = true;
}

void FftPitchCore::SetPitch(float value) { pitch_target = value; }
void FftPitchCore::SetVolume(float value) { volume_target = value; }
void FftPitchCore::SetRunning(bool value) { running = value; }
bool FftPitchCore::IsActive() const { return running || gate > 0.0f; }
uint32_t FftPitchCore::Transport() const { return transport; }
uint32_t FftPitchCore::GeneratedHops() const { return generated; }
float FftPitchCore::Pitch() const { return pitch_now; }
uint32_t FftPitchCore::TransientResets() const { return transient_resets; }

float FftPitchCore::WrapPhase(float phase)
{
    return phase - Tau * std::round(phase / Tau);
}

void FftPitchCore::ReadReferenceBlock(int16_t *destination, uint32_t first, uint32_t count) const
{
    for (uint32_t i = 0; i < count; ++i)
    {
        destination[i] = source[first];
        if (++first == source_count)
        {
            first = 0;
        }
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

void FftPitchCore::Generate()
{
    ++generated;
    uint32_t position = transport;
    float frame_energy = 0.0f;
    for (uint32_t i = 0; i < Size; ++i)
    {
        fft[2 * i] = source[position] * window[i];
        frame_energy += fft[2 * i] * fft[2 * i];
        fft[2 * i + 1] = 0.0f;
        if (++position == source_count)
        {
            position = 0;
        }
    }
    Transform(false);
    std::fill_n(magnitude, Bins, 0.0f);
    std::fill_n(frequency_sum, Bins, 0.0f);
    std::fill_n(strongest, Bins, 0.0f);
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
        const float deviation = first_frame ? 0.0f : WrapPhase(phase - previous_phase[bin] - expected);
        const float frequency = pitch_now == 1.0f ? static_cast<float>(bin) : (bin + deviation * (Size / (Tau * Hop))) * pitch_now;
        previous_phase[bin] = phase;
        const uint32_t destination = static_cast<uint32_t>(bin * pitch_now + 0.5f);
        // Spectral band limiting: never fold shifted components above Nyquist back into the output.
        if (destination >= Bins || frequency < 0.0f || frequency > Size / 2)
        {
            continue;
        }
        magnitude[destination] += amplitude;
        frequency_sum[destination] += amplitude * frequency;
        if (amplitude > strongest[destination])
        {
            strongest[destination] = amplitude;
            initial_phase[destination] = phase;
            dominant_source[destination] = static_cast<uint16_t>(bin);
        }
    }
    // Positive spectral flux detects new attacks, including changes in spectral shape.
    // A windowed RMS floor rejects silence; four hops suppress repeated resets of one attack.
    if (transient_holdoff > 0)
    {
        --transient_holdoff;
    }
    const bool transient = !first_frame && transient_holdoff == 0 && frame_energy > Size * 64.0f * 64.0f && positive_flux > 0.35f * spectral_sum;
    if (transient)
    {
        transient_holdoff = 4;
        ++transient_resets;
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
    // Advance all phases before locking so no bin depends on traversal order.
    for (uint32_t bin = 0; bin < Bins; ++bin)
    {
        if (magnitude[bin] > 0.0f)
        {
            const float frequency = frequency_sum[bin] / magnitude[bin];
            advanced_phase[bin] = first_frame || transient || pitch_now == 1.0f ? initial_phase[bin] : WrapPhase(synthesis_phase[bin] + Tau * frequency * Hop / Size);
        }
    }
    std::fill_n(fft, 2 * Size, 0.0f);
    for (uint32_t bin = 0; bin < Bins; ++bin)
    {
        if (magnitude[bin] == 0.0f)
        {
            continue;
        }
        synthesis_phase[bin] = advanced_phase[bin];
        if (peak_count > 0 && !first_frame && !transient && pitch_now != 1.0f)
        {
            const uint32_t peak = peak_owner[dominant_source[bin]];
            const uint32_t anchor = static_cast<uint32_t>(peak * pitch_now + 0.5f);
            // A removed or collision-displaced peak cannot anchor this region.
            if (anchor < Bins && magnitude[anchor] > 0.0f && dominant_source[anchor] == peak)
            {
                synthesis_phase[bin] = WrapPhase(advanced_phase[anchor] + initial_phase[bin] - previous_phase[peak]);
            }
        }
        const float real = magnitude[bin] * std::cos(synthesis_phase[bin]);
        const float imag = magnitude[bin] * std::sin(synthesis_phase[bin]);
        fft[2 * bin] = real;
        fft[2 * bin + 1] = bin == 0 || bin == Size / 2 ? 0.0f : imag;
        if (bin > 0 && bin < Size / 2)
        {
            fft[2 * (Size - bin)] = real;
            fft[2 * (Size - bin) + 1] = -imag;
        }
    }
    Transform(true);
    for (uint32_t i = 0; i < Size; ++i)
    {
        accumulation[(head + i) & (Size - 1)] += fft[2 * i] * window[i] * (2.0f / 3.0f);
    }
    first_frame = false;
}

int16_t FftPitchCore::Render()
{
    if (!IsActive())
    {
        initialized = false;
        return 0;
    }
    if (!initialized)
    {
        std::fill_n(previous_phase, Bins, 0.0f);
        std::fill_n(previous_magnitude, Bins, 0.0f);
        std::fill_n(synthesis_phase, Bins, 0.0f);
        std::fill_n(accumulation, Size, 0.0f);
        first_frame = true;
        head = 0;
        transport = 0;
        until_frame = 0;
        pitch_now = pitch_target;
        initialized = true;
    }
    pitch_now += (pitch_target - pitch_now) * (1.0f / 256.0f);
    volume_now += (volume_target - volume_now) * (1.0f / 256.0f);
    if (until_frame == 0)
    {
        Generate();
        until_frame = Hop;
    }
    --until_frame;
    gate = running ? std::min(1.0f, gate + 1.0f / 221.0f) : std::max(0.0f, gate - 1.0f / 221.0f);
    const float value = accumulation[head] * gate * volume_now;
    accumulation[head] = 0.0f;
    head = (head + 1) & (Size - 1);
    if (++transport == source_count)
    {
        transport = 0;
    }
    return static_cast<int16_t>(std::clamp(value, -32768.0f, 32767.0f));
}
