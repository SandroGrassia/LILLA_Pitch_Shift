#include "FftAttackCache.h"
#ifdef __IMXRT1062__
#include <Arduino.h>
#define ATTACK_RAM DMAMEM
#define ATTACK_BUILDER_RAM EXTMEM
#else
#define ATTACK_RAM
#define ATTACK_BUILDER_RAM
#endif

ATTACK_RAM FftAttackCache::Entry FftAttackCache::entries[FftAttackCache::Capacity];
ATTACK_BUILDER_RAM FftPitchCore FftAttackCache::builder;
FftAttackCache::Request FftAttackCache::pending = {};
bool FftAttackCache::building = false;
uint32_t FftAttackCache::next = 0;
uint32_t FftAttackCache::hits = 0;

bool FftAttackCache::Equal(const Request &a, const Request &b)
{
    return a.samples == b.samples && a.count == b.count && a.source_first == b.source_first && a.from == b.from && a.end == b.end && a.mix == b.mix && a.mode == b.mode;
}

bool FftAttackCache::Ready(const Request &request)
{
    for (const auto &entry : entries)
    {
        if (entry.ready && Equal(entry.key, request))
        {
            return true;
        }
    }
    return false;
}

void FftAttackCache::Prepare(const Request &request)
{
    if (Ready(request))
    {
        return;
    }
    if (!building || !Equal(pending, request))
    {
        pending = request;
        builder.StartNote(request.samples, request.count, 1.0f, 1.0f);
        builder.SetRegion(request.from - request.source_first, request.end - request.source_first, request.mix);
        builder.SetMode(request.mode);
        building = true;
    }
    if (builder.PrepareStartStep())
    {
        Entry &entry = entries[next];
        entry.ready = false;
        builder.SaveInitialAnalysis(entry.data);
        entry.key = pending;
        entry.ready = true;
        next = (next + 1u) % Capacity;
        building = false;
    }
}

bool FftAttackCache::Restore(const Request &request, FftPitchCore &voice)
{
    for (const auto &entry : entries)
    {
        if (entry.ready && Equal(entry.key, request))
        {
            voice.LoadInitialAnalysis(entry.data);
            ++hits;
            return true;
        }
    }
    return false;
}

void FftAttackCache::Invalidate()
{
    for (auto &entry : entries)
    {
        entry.ready = false;
    }
    building = false;
}

uint32_t FftAttackCache::Hits()
{
    return hits;
}
