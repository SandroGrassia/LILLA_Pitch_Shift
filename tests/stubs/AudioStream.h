#pragma once
#include <cstdint>
#include <cstring>
constexpr uint32_t AUDIO_BLOCK_SAMPLES = 128;
constexpr float AUDIO_SAMPLE_RATE_EXACT = 44100.0f;
struct audio_block_t
{
    int16_t data[AUDIO_BLOCK_SAMPLES];
};
class AudioStream
{
public:
    AudioStream(unsigned, void *) {}
    virtual void update() = 0;
    inline static audio_block_t transmitted = {};
    inline static bool fail_allocation = false;
    inline static unsigned transmissions = 0;
protected:
    audio_block_t storage;
    audio_block_t *allocate()
    {
        return fail_allocation ? nullptr : &storage;
    }
    void transmit(audio_block_t *block)
    {
        std::memcpy(&transmitted, block, sizeof(transmitted));
        ++transmissions;
    }
    void release(audio_block_t *) {}
};
