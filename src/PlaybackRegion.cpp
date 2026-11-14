#include "PlaybackRegion.h"
#include <algorithm>

void PlaybackRegion::Reset(uint32_t samples)
{
    count = samples;
    cross_mix = 0;
    minimum = std::min<uint32_t>(100u, samples);
    first = 0;
    last = samples - 1;
    slice = false;
}

void PlaybackRegion::RotateStep(int delta)
{
    speed = static_cast<int>(std::clamp<int64_t>(static_cast<int64_t>(speed) + delta, 0, 5));
}

void PlaybackRegion::ResetStep()
{
    speed = 5;
}

void PlaybackRegion::ToggleSlice()
{
    slice = !slice;
}

uint32_t PlaybackRegion::Step() const
{
    constexpr uint32_t steps[] = {1, 10, 100, 1000, 10000};
    return speed == 5 ? std::max<uint32_t>(1u, Length() / 16u) : steps[speed];
}

void PlaybackRegion::MoveFrom(int delta)
{
    const uint32_t length = Length();
    const uint32_t maximum = slice ? count - length : last - minimum + 1;
    first = static_cast<uint32_t>(std::clamp<int64_t>(static_cast<int64_t>(first) + static_cast<int64_t>(delta) * Step(), 0, maximum));
    if (slice)
    {
        last = first + length - 1;
    }
    cross_mix = CrossMix();
}

void PlaybackRegion::MoveTo(int delta)
{
    last = static_cast<uint32_t>(std::clamp<int64_t>(static_cast<int64_t>(last) + static_cast<int64_t>(delta) * Step(), first + minimum - 1, count - 1));
    cross_mix = CrossMix();
}

void PlaybackRegion::ResetFrom()
{
    if (slice)
    {
        last -= first;
    }
    first = 0;
}

uint32_t PlaybackRegion::CrossMixMax() const
{
    // Lilla main: BLOCK_MIN=674, wavetable Noclick max=42, NOCLICK_DIM=300.
    return std::min<uint32_t>(Length() <= 674u ? 42u : 300u, Length() / 2u);
}

uint32_t PlaybackRegion::CrossMix() const
{
    const uint32_t value = std::min(cross_mix, CrossMixMax());
    return value < 2 ? 0 : value;
}

void PlaybackRegion::MoveCrossMix(int delta)
{
    cross_mix = CrossMix();
    const uint32_t maximum = CrossMixMax();
    while (delta > 0 && cross_mix < maximum)
    {
        const uint32_t increment = cross_mix >= 90 ? 10 : cross_mix >= 30 ? 4 : 2;
        cross_mix = std::min(cross_mix + increment, maximum);
        --delta;
    }
    while (delta < 0 && cross_mix > 0)
    {
        const uint32_t decrement = cross_mix <= 30 ? 2 : cross_mix <= 90 ? 4 : 10;
        cross_mix = cross_mix > decrement ? cross_mix - decrement : 0;
        ++delta;
    }
    if (cross_mix < 2)
    {
        cross_mix = 0;
    }
}
