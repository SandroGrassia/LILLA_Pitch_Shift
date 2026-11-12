#include "WsolaCore.h"
#include <algorithm>
#include <cmath>
#include <cstring>

void WsolaCore::Attach(const int16_t *samples, uint32_t count)
{
    source = samples;
    source_count = count;
}

void WsolaCore::SetPitch(float value)
{
    pitch_target = value;
}

void WsolaCore::SetWindow(uint32_t samples)
{
    if (samples != requested_window && initialized)
    {
        geometry_pending = true;
    }
    requested_window = samples;
}

void WsolaCore::SetOverlap(uint32_t percent)
{
    if (percent != requested_overlap && initialized)
    {
        geometry_pending = true;
    }
    requested_overlap = percent;
}

void WsolaCore::SetSearch(uint32_t samples)
{
    search_radius = samples;
}

void WsolaCore::SetVolume(float value)
{
    volume_target = value;
}

void WsolaCore::SetRunning(bool value)
{
    running = value;
}

uint32_t WsolaCore::Transport() const
{
    return transport;
}

float WsolaCore::Pitch() const
{
    return pitch_now;
}

float WsolaCore::Correlation() const
{
    return last_correlation;
}

int32_t WsolaCore::Offset() const
{
    return last_offset;
}

int16_t WsolaCore::ReferenceSample() const
{
    return running || gate > 0.0f ? source[transport] : 0;
}

bool WsolaCore::IsActive() const
{
    return running || gate > 0.0f;
}

uint32_t WsolaCore::GeneratedHops() const
{
    return generated_hops;
}

void WsolaCore::ReadReferenceBlock(int16_t *destination, uint32_t first, uint32_t count) const
{
    CopySource(destination, first, count);
}

uint32_t WsolaCore::Wrap(int64_t position) const
{
    return static_cast<uint32_t>((position % source_count + source_count) % source_count);
}

void WsolaCore::CopySource(int16_t *destination, uint32_t position, uint32_t count) const
{
    while (count > 0)
    {
        const uint32_t chunk = std::min(count, source_count - position);
        std::memcpy(destination, source + position, chunk * sizeof(int16_t));
        destination += chunk;
        count -= chunk;
        position = 0;
    }
}

float WsolaCore::Score(uint32_t offset, uint32_t stride) const
{
    float a = 0.0f;
    float b = 0.0f;
    float aa = 0.0f;
    float bb = 0.0f;
    float ab = 0.0f;
    uint32_t count = 0;
    for (uint32_t i = 0; i < overlap; i += stride)
    {
        const float x = tail[i] * (1.0f / 32768.0f);
        const float y = search[offset + i] * (1.0f / 32768.0f);
        a += x;
        b += y;
        aa += x * x;
        bb += y * y;
        ab += x * y;
        ++count;
    }
    const float variance_a = aa - a * a / count;
    const float variance_b = bb - b * b / count;
    if (variance_a <= 0.000001f || variance_b <= 0.000001f)
    {
        return -1.0f; // Silence or constant signal: prefer the nominal position.
    }
    return (ab - a * b / count) / std::sqrt(variance_a * variance_b);
}

void WsolaCore::Generate(uint32_t destination)
{
    ++generated_hops;
    if (overlap != hop)
    {
        for (uint32_t i = 0; i < overlap; ++i)
        {
            tail[i] = static_cast<int16_t>(accumulated[i] / weights[i]);
        }
    }
    // One synthesis hop ahead. Anchor to the actual output transport, not the last
    // search match: local alignment errors never accumulate into tempo drift.
    const float ahead = (hop - read_position) / pitch_now;
    const uint32_t nominal = Wrap(static_cast<int64_t>(transport) + static_cast<int64_t>(std::lround(ahead)));
    CopySource(search, Wrap(static_cast<int64_t>(nominal) - search_radius), window + 2u * search_radius);
    uint32_t best = search_radius;
    const uint32_t coarse_stride = std::max<uint32_t>(1u, overlap / 128u);
    float best_score = Score(best, coarse_stride);
    if (search_radius > 0 && std::fabs(pitch_now - 1.0f) > 0.00001f)
    {
        for (uint32_t candidate = 0; candidate <= 2u * search_radius; candidate += 8u)
        {
            const float penalty = 0.002f * std::abs(static_cast<int32_t>(candidate) - static_cast<int32_t>(search_radius)) / search_radius;
            const float score = Score(candidate, coarse_stride) - penalty;
            if (score > best_score)
            {
                best = candidate;
                best_score = score;
            }
        }
        const uint32_t first = best > 8u ? best - 8u : 0u;
        const uint32_t last = std::min(best + 8u, 2u * search_radius);
        const uint32_t fine_stride = std::max<uint32_t>(1u, overlap / 256u);
        best_score = Score(best, fine_stride) - 0.002f * std::abs(static_cast<int32_t>(best) - static_cast<int32_t>(search_radius)) / search_radius;
        for (uint32_t candidate = first; candidate <= last; ++candidate)
        {
            const float penalty = 0.002f * std::abs(static_cast<int32_t>(candidate) - static_cast<int32_t>(search_radius)) / search_radius;
            const float score = Score(candidate, fine_stride) - penalty;
            if (score > best_score)
            {
                best = candidate;
                best_score = score;
            }
        }
    }
    last_offset = static_cast<int32_t>(best) - static_cast<int32_t>(search_radius);
    last_correlation = Score(best, std::max<uint32_t>(1u, overlap / 256u));
    if (overlap != hop)
    {
        MixFragment(destination, best);
        return;
    }
    for (uint32_t i = 0; i < hop; ++i)
    {
        const float incoming = static_cast<float>(i) / (hop - 1u);
        const float value = tail[i] + incoming * (search[best + i] - tail[i]);
        blocks[destination][i] = static_cast<int16_t>(value);
        tail[i] = search[best + hop + i];
    }
}

void WsolaCore::Prime()
{
    window = requested_window;
    overlap = window * requested_overlap / 100u;
    hop = window - overlap;
    current = 0;
    read_position = 0.0f;
    CopySource(blocks[current], transport, hop);
    CopySource(tail, Wrap(static_cast<int64_t>(transport) + hop), overlap);
    if (overlap != hop)
    {
        std::fill_n(accumulated, window, 0.0f);
        std::fill_n(weights, window, 0.0f);
        for (uint32_t i = 0; i < overlap; ++i)
        {
            // The first fragment has no incoming ramp; the final weight stays positive for correlation.
            weights[i] = static_cast<float>(overlap - i) / overlap;
            accumulated[i] = tail[i] * weights[i];
        }
    }
    Generate(1u);
    initialized = true;
}

int16_t WsolaCore::Render()
{
    if (!running && gate == 0.0f)
    {
        initialized = false;
        geometry_pending = false;
        return 0;
    }
    if (!initialized)
    {
        transport = 0;
        pitch_now = pitch_target;
        Prime();
    }
    pitch_now += (pitch_target - pitch_now) * (1.0f / 256.0f);
    volume_now += (volume_target - volume_now) * (1.0f / 256.0f);
    if (running && !geometry_pending)
    {
        gate = std::fmin(1.0f, gate + 1.0f / 221.0f);
    }
    else
    {
        gate = std::fmax(0.0f, gate - 1.0f / 221.0f);
    }
    if (geometry_pending && gate == 0.0f)
    {
        Prime();
        geometry_pending = false;
    }
    while (read_position >= hop)
    {
        read_position -= hop;
        current ^= 1u;
        Generate(current ^ 1u);
    }
    const uint32_t index = static_cast<uint32_t>(read_position);
    const float fraction = read_position - index;
    const int16_t next = index + 1u < hop ? blocks[current][index + 1u] : blocks[current ^ 1u][0];
    const float value = blocks[current][index] + fraction * (next - blocks[current][index]);
    read_position += pitch_now;
    ++transport;
    if (transport == source_count)
    {
        transport = 0;
    }
    const float output = value * volume_now * gate;
    return static_cast<int16_t>(std::fmax(-32768.0f, std::fmin(32767.0f, output)));
}

void WsolaCore::MixFragment(uint32_t destination, uint32_t best)
{
    for (uint32_t i = 0; i < window; ++i)
    {
        const float incoming = static_cast<float>(i) / overlap;
        const float outgoing = static_cast<float>(window - i) / overlap;
        const float weight = std::fmin(1.0f, std::fmin(incoming, outgoing));
        accumulated[i] += search[best + i] * weight;
        weights[i] += weight;
    }
    for (uint32_t i = 0; i < hop; ++i)
    {
        const float value = accumulated[i] / weights[i];
        blocks[destination][i] = static_cast<int16_t>(std::fmax(-32768.0f, std::fmin(32767.0f, value)));
    }
    for (uint32_t i = 0; i < overlap; ++i)
    {
        accumulated[i] = accumulated[i + hop];
        weights[i] = weights[i + hop];
    }
    std::fill(accumulated + overlap, accumulated + window, 0.0f);
    std::fill(weights + overlap, weights + window, 0.0f);
}
