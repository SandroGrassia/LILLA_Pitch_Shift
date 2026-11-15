int main()
{
    std::vector<int16_t> source(40000);
    for (unsigned i = 0; i < source.size(); ++i)
    {
        source[i] = static_cast<int16_t>(10000 * std::sin(i * 0.031) + 4000 * std::sin(i * 0.173));
    }
    // Exercise the production Flash cache and PSRAM offset reader across all six modes.
    for (int storage : {Flash, Psram})
    {
        for (uint8_t mode = 0; mode < 6; ++mode)
        {
            AudioPlayer player;
            player.flash = source;
            player.source_now = {storage, 333, 3000, source.data() + 333};
            player.A_Flash_sample = 333;
            player.B_Flash_sample = 3332;
            player.delta_Noclick = 300;
            player.fft_mode_wait = mode;
            player.pitch = 1.7f;
            player.Start_fft();
            FftPitchCore reference;
            reference.StartNote(source.data(), source.size(), 1.7f, 1.0f);
            reference.SetRegion(333, 3333, 300);
            reference.SetMode(AudioPlayer::Fft_mode(mode));
            for (unsigned i = 0; i < 12000; ++i)
            {
                assert(player.fft_core.Render() == reference.Render());
            }
            assert(player.transfers < 600); // A mixed seam must not reread Flash twice for every sample.
        }
    }
    for (float pitch : {0.22272468f, 1.0f, 1.7f, 2.24492410f})
    {
        Recorder recorder;
        std::vector<int16_t> ring(LS_buffer_dim, 30000);
        AudioPlayer live;
        live.LS_flag = true;
        live.Live_Sampler = &recorder;
        live.FIFO = ring.data();
        live.pitch = pitch;
        live.Start_fft();
        assert(live.fft_live_waiting);
        assert(AudioPlayer::Read_fft_source(&live, 0) == 0);
        std::copy_n(source.data(), AudioPlayer::Live_fft_margin, ring.data());
        recorder.Q_sample = AudioPlayer::Live_fft_margin - 1;
        assert(live.Prepare_fft_live());
        assert(live.fft_core.Transport() == 0);
        FftPitchCore reference;
        reference.StartNote(source.data(), source.size(), pitch, 1.0f);
        unsigned written = AudioPlayer::Live_fft_margin;
        unsigned rendered = 0;
        for (unsigned block = 0; block < 150; ++block)
        {
            for (unsigned i = 0; i < 128; ++i)
            {
                assert(live.fft_core.Render() == reference.Render());
                ++rendered;
            }
            for (unsigned i = 0; i < 128; ++i)
            {
                ring[(written + i) % ring.size()] = source[written + i];
            }
            written += 128;
            recorder.Q_sample = (written - 1) % ring.size();
            recorder.first_write_flag = written <= ring.size();
        }
        recorder.writing = false;
        assert(live.Prepare_fft_live());
        reference.FinishForwardAt(written);
        for (unsigned i = rendered; i < written; ++i)
        {
            assert(live.fft_core.Render() == reference.Render());
        }
        assert(!live.fft_core.IsActive());
    }
    // Loop start is deferred while its full region is unrecorded.
    Recorder recorder;
    std::vector<int16_t> ring(LS_buffer_dim, 1);
    AudioPlayer loop;
    loop.LS_flag = true;
    loop.Live_Sampler = &recorder;
    loop.FIFO = ring.data();
    loop.fft_mode_wait = LOOP_FWD_REV;
    LS_XY_lock = true;
    LS_X_sample = 2000;
    recorder.Q_sample = 1279;
    loop.Start_fft();
    assert(loop.fft_live_waiting);
    recorder.Q_sample = 4095;
    assert(loop.Prepare_fft_live() && !loop.fft_live_waiting);
    // Exercise the production render wrapper: queued FFT starts must preserve ADSR attacks.
    AudioPlayer chord[4];
    for (auto &voice : chord)
    {
        voice.source_now = {Psram, 0, static_cast<uint32_t>(source.size()), source.data()};
        voice.fft_mode_wait = LOOP_FWD;
        voice.Start_fft();
    }
    for (unsigned cycle = 0; cycle < 4; ++cycle)
    {
        audio_update_cycle = cycle;
        for (auto &voice : chord)
        {
            voice.Update_fft();
        }
        for (unsigned voice = 0; voice < 4; ++voice)
        {
            assert((chord[voice].fft_core.GeneratedHops() > 0) == (voice <= cycle));
            assert((chord[voice].envelope.calls > 0) == (voice <= cycle));
        }
    }
    chord[0].powered = false;
    chord[0].envelope.phase = 3;
    chord[0].envelope.gain = 0;
    ++audio_update_cycle;
    chord[0].Update_fft();
    assert(chord[0].state == AudioPlayer::PlayerState::Idle && chord[0].closed);

    // A running reverse Live voice can enter forward follow mode and then drain on STOP.
    LS_XY_lock = false;
    recorder.Q_sample = 4095;
    recorder.writing = true;
    AudioPlayer switching;
    switching.LS_flag = true;
    switching.Live_Sampler = &recorder;
    switching.FIFO = ring.data();
    switching.fft_mode_wait = ONCE_REV;
    switching.Start_fft();
    ++audio_update_cycle;
    switching.Update_fft();
    switching.fft_mode_edit = ONCE_FWD;
    switching.main_settings_editing_flag = true;
    ++audio_update_cycle;
    switching.Update_fft();
    assert(switching.fft_live_follow);
    recorder.writing = false;
    for (unsigned cycle = 0; cycle < 80 && switching.state != AudioPlayer::PlayerState::Idle; ++cycle)
    {
        ++audio_update_cycle;
        switching.Update_fft();
    }
    assert(switching.state == AudioPlayer::PlayerState::Idle);


    std::puts("PASS: actual Lilla FFT adapter: Flash/PSRAM, all modes, bounded cache reads, live startup, circular overwrite, first-fill guard and stop drain at pitch extremes.");
}
