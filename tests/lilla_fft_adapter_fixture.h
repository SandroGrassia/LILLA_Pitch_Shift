// Hardware shim for testing the actual adapter methods extracted from AudioPlayer.cpp.
#include "FftPitchCore.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
#include <cstring>
constexpr int ONCE_FWD = 0, ONCE_REV = 1, LOOP_FWD = 2, LOOP_FWD_REV = 3, LOOP_REV_FWD = 4, LOOP_REV = 5;
constexpr int Flash = 0, Psram = 1;
int LS_buffer_dim = 8192;
bool LS_XY_lock = false;
int LS_X_sample = 0, LS_X_delta = 0, LS_XY_delta = 999;
struct Recorder
{
    int Q_sample = -1;
    bool first_write_flag = true;
    bool writing = true;
    bool Is_writing() { return writing; }
};
constexpr unsigned AUDIO_BLOCK_SAMPLES = 128;
constexpr unsigned SAMPLES_VOLUME = 5000;
constexpr float AUDIO_SAMPLE_RATE_EXACT = 44100.0f;
constexpr float VIBRATO_STEP = 0.1f;
uint32_t audio_update_cycle = 0;
struct audio_block_t { int16_t data[128]; };
int16_t Lilla_saturate16(int32_t value) { return static_cast<int16_t>(std::clamp<int32_t>(value, -32768, 32767)); }
struct Envelope
{
    unsigned calls = 0;
    float gain = 1;
    int phase = 0;
    float Get_gain() { ++calls; return gain; }
    int Get_phase() { return phase; }
};
struct Filter { bool LFO_connect = false; void Update() {} };
struct Tone { void Process(int16_t *, int, float, int, float) {} };
class AudioPlayer
{
public:
    uint8_t identity = 0;
    static uint32_t fft_first_frame_cycle;
    FftPitchCore fft_core;
    static constexpr uint32_t Live_fft_margin = FftPitchCore::SourceLookahead;
    bool fft_live_waiting = false, fft_live_follow = false, fft_live_restart = false;
    bool LS_flag = false, live_unrecorded_notice = false, finished = false;
    Recorder *Live_Sampler = nullptr;
    int16_t *FIFO = nullptr;
    uint8_t fft_mode_wait = ONCE_FWD;
    int A_Flash_sample = 0, B_Flash_sample = 5999;
    uint16_t delta_Noclick = 0;
    float pitch = 1.0f;
    int16_t fft_read_cache[2][256] = {};
    uint32_t fft_cache_first[2] = {UINT32_MAX, UINT32_MAX};
    uint32_t fft_cache_count[2] = {};
    uint8_t fft_cache_recent = 0;
    struct Source { int storage = Flash; uint32_t first_sample = 0, samples = 0; const int16_t *psram_ptr = nullptr; } source_now;
    std::vector<int16_t> flash;
    unsigned transfers = 0;
    void Read_samples(int16_t *out, unsigned first, unsigned count)
    {
        assert(first + count <= flash.size());
        std::copy_n(flash.data() + first, count, out);
        ++transfers;
    }
    void Update_pitch() {}
    void Finish_block() { finished = true; }
    static int16_t Read_fft_source(void *, uint32_t);
    static FftPitchCore::PlayMode Fft_mode(uint8_t);
    void Configure_fft_live_region(uint8_t, int &, int &);
    bool Prepare_fft_live();
    void Start_fft();
    void Update_fft();
    enum class PlayerState { Idle, Playing, Released };
    PlayerState state = PlayerState::Playing;
    bool powered = true;
    bool warmup_for_play_again_flag = false, restart_flag = false, pitch_tune_flag = false, pitch_bend_flag = false, vibrato_flag = false;
    bool main_settings_editing_flag = false, volume_gain_warmup_flag = false, volume_flag = false, pan_gain_warmup_flag = false, pan_flag = false;
    bool set_effects_flag = false, resolution_flag = false, resolution_flag_wait = false, downsampling_flag = false, downsampling_flag_wait = false;
    bool use_Wavetable = false, use_Wavetable_E = false, rendered_block_valid = false, VCF_connect = false, VCF_modulated = false;
    float pitch_tune = 1, pitch_tune_wait = 1, pitch_bend = 1, pitch_bend_wait = 1, pitch_vibrato = 1;
    float vibrato_array_element_float = 0, modulation_depth = 0, volume_gain = 1, velocity_gain = 1, ADSR_gain = 0;
    float pan_gain_L = 1, pan_gain_R = 1, pan_gain_L_wait = 1, pan_gain_R_wait = 1, K_pan_gain_L = 0, K_pan_gain_R = 0;
    float K_resolution_step = 1, K_resolution_step_wait = 1, pitch_limit = 2.2449241f, pitch_limit_E = 2.2449241f;
    float a_first_sample = 0;
    float *vibrato_array_ptr = nullptr;
    uint8_t *vibrato_array_last_element_ptr = nullptr;
    uint8_t fft_mode_edit = ONCE_FWD, mode_player = ONCE_FWD, mode_player_E = ONCE_FWD, tone_filter_level = 0;
    uint8_t tables_bank_mask = 0, tables_bank_mask_E = 0;
    int pan_int = 0, pan_int_wait = 0, downsampling = 1, downsampling_wait = 1, sliding = 0, VCF_pivot_steps = 0;
    int A_Flash_sample_E = 0, B_Flash_sample_E = 5999, delta_Noclick_E = 0;
    uint32_t samples_counter = 0, JP0 = 0;
    int16_t *Wavetable_ptr = nullptr, *Wavetable_E_ptr = nullptr, *Noclick_ptr = nullptr, *Noclick_E_ptr = nullptr;
    int16_t stored_sample = 0, block[128] = {};
    Envelope envelope;
    Envelope *ADSR = &envelope;
    Filter VCF, LFO;
    Tone tone_filter;
    audio_block_t buffers[2] = {}, sent[2] = {};
    unsigned allocations = 0;
    bool closed = false;
    audio_block_t *allocate() { return &buffers[(allocations++) % 2]; }
    void release(audio_block_t *) {}
    void transmit(audio_block_t *value, int channel) { sent[channel] = *value; }
    bool isPoweredOn() { return powered; }
    bool Is_finishing() { return finished; }
    void Update_volume_gain() {}
    void Update_pan_gain() {}
    void Send_LFO_to_VCF() {}
    void Close_source() { closed = true; }
    void My_LED(bool) {}
    void Start_playing() { Start_fft(); }

};
