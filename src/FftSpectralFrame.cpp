/*
   LILLA Audio Sampler
   Author: Sandro Grassia (info@lillasampler.it)
   www.lillasampler.it
*/

#include "FftSpectralFrame.h"
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

bool FftSpectralFrame::tables_ready = false;
float FftSpectralFrame::window[FftSpectralFrame::Size] = {};

FftSpectralFrame::FftSpectralFrame()
    : fft_storage(new float[2 * Size + 7]{})
{
    const uintptr_t address = reinterpret_cast<uintptr_t>(fft_storage.get());
    const uintptr_t aligned_address = (address + 31u) & ~uintptr_t(31u);
    fft = reinterpret_cast<float *>(aligned_address);
    if (!tables_ready)
    {
        for (uint32_t i = 0; i < Size; ++i)
        {
            window[i] = 0.5f - 0.5f * std::cos(Tau * i / Size);
        }
        tables_ready = true;
    }
}

float FftSpectralFrame::WrapPhase(float phase)
{
    return phase - Tau * std::round(phase / Tau);
}

void FftSpectralFrame::Transform(bool inverse)
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
        for (uint32_t i = 0; i < 2 * Size; ++i)
        {
            fft[i] /= Size;
        }
    }
#endif
}

void FftSpectralFrame::Reset()
{
    std::fill_n(previous_phase, Bins, 0.0f);
    std::fill_n(previous_magnitude, Bins, 0.0f);
    std::fill_n(synthesis_phase, Bins, 0.0f);
    std::fill_n(accumulation, Size, 0.0f);
    first_frame = true;
    analysis_ready = false;
    frame_transient = false;
    transient_holdoff = 0;
    head = 0;
}

void FftSpectralFrame::Input(uint32_t index, float value)
{
    fft[2 * index] = value * window[index];
    fft[2 * index + 1] = 0.0f;
}

float FftSpectralFrame::LowWeight(float frequency)
{
    const float x = std::clamp((frequency - 200.0f) / 200.0f, 0.0f, 1.0f);
    return 1.0f - x * x * (3.0f - 2.0f * x);
}

float FftSpectralFrame::Take()
{
    const float value = accumulation[head];
    accumulation[head] = 0.0f;
    head = (head + 1u) & (Size - 1u);
    return value;
}

void FftSpectralFrame::Analyze(bool reanchor)
{
    ++analyzed;
    float frame_energy = 0.0f;
    for (uint32_t i = 0; i < Size; ++i)
    {
        frame_energy += fft[2 * i] * fft[2 * i];
    }
    Transform(false);
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
        const float deviation = (first_frame || reanchor) ? 0.0f : WrapPhase(phase - previous_phase[bin] - expected);
        analysis_frequency[bin] = bin + deviation * (Size / (Tau * Hop));
        previous_phase[bin] = phase;
    }
    if (transient_holdoff > 0)
    {
        --transient_holdoff;
    }
    frame_transient = reanchor || (!first_frame && transient_holdoff == 0 && frame_energy > Size * 64.0f * 64.0f && positive_flux > 0.35f * spectral_sum);
    if (frame_transient)
    {
        transient_holdoff = 4;
        ++transient_resets;
    }
    analysis_ready = true;
}

void FftSpectralFrame::Generate(float pitch_now, Band band)
{
    ++generated;
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
    // At Fs/4 the window center is 1536 source samples later; compensate that reset-phase offset.
    const bool reset_phase = first_frame || frame_transient || pitch_now == 1.0f;
    for (uint32_t index = 0; index < peak_count; ++index)
    {
        const uint32_t peak = peaks[index];
        const float shift = (pitch_now - 1.0f) * analysis_frequency[peak];
        advanced_phase[peak] = reset_phase ? WrapPhase(previous_phase[peak] + (band == Band::Low ? 0.875f : 0.5f) * Tau * shift) : WrapPhase(synthesis_phase[peak] + Tau * analysis_frequency[peak] * pitch_now * Hop / Size);
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
        const float input_hz = analysis_frequency[peak] * (44100.0f / (band == Band::Low ? 4096.0f : 1024.0f));
        const float low_weight = LowWeight(input_hz);
        const float band_weight = band == Band::Full ? 1.0f : band == Band::Low ? low_weight : 1.0f - low_weight;
        if (band_weight == 0.0f)
        {
            continue; // Phase history remains current even for regions assigned to the other band.
        }
        const float real = previous_magnitude[bin] * band_weight * std::cos(centered_phase);
        const float imag = previous_magnitude[bin] * band_weight * std::sin(centered_phase);
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
    Transform(true);
    for (uint32_t i = 0; i < Size; ++i)
    {
        accumulation[(head + i) & (Size - 1)] += fft[2 * i] * window[i] * (2.0f / 3.0f);
    }
    first_frame = false;
    analysis_ready = false;
}
