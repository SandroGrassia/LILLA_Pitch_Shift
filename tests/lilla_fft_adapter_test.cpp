int main()
{
    std::vector<int16_t> source(40000);
    for (unsigned i = 0; i < source.size(); ++i)
    {
        source[i] = static_cast<int16_t>(10000 * std::sin(i * 0.031) + 4000 * std::sin(i * 0.173));
    }
    // Cached initial analysis is pitch-independent and matches immediate rendering in every mode.
    for (uint8_t mode = 0; mode < 6; ++mode)
    {
        FftAttackCache::Invalidate();
        const FftAttackCache::Request key = {source.data() + 333, 3000, 333, 333, 3333, 300, AudioPlayer::Fft_mode(mode)};
        for (unsigned step = 0; step < 7; ++step)
        {
            FftAttackCache::Prepare(key);
        }
        assert(FftAttackCache::Ready(key));
        auto changed = key;
        ++changed.from;
        assert(!FftAttackCache::Ready(changed));
        changed = key;
        changed.mix = 0;
        assert(!FftAttackCache::Ready(changed));
        for (float pitch : {0.5f, 1.0f, 1.7f, 2.24492410f})
        {
            FftPitchCore cached;
            FftPitchCore reference;
            cached.StartNote(source.data(), source.size(), pitch, 0.5f);
            reference.StartNote(source.data(), source.size(), pitch, 0.5f);
            cached.SetRegion(333, 3333, 300);
            reference.SetRegion(333, 3333, 300);
            cached.SetMode(AudioPlayer::Fft_mode(mode));
            reference.SetMode(AudioPlayer::Fft_mode(mode));
            assert(FftAttackCache::Restore(key, cached));
            assert(cached.PrepareStartStep());
            for (unsigned i = 0; i < 5000; ++i)
            {
                assert(cached.Render() == reference.Render());
            }
        }
        FftAttackCache::Invalidate();
        assert(!FftAttackCache::Ready(key));
    }
    // A ready PSRAM attack renders in its first allowed callback, without six preparation blocks.
    const FftAttackCache::Request fast_key = {source.data(), static_cast<uint32_t>(source.size()), 0, 0, 6000, 0, FftPitchCore::PlayMode::LoopForward};
    for (unsigned step = 0; step < 7; ++step)
    {
        FftAttackCache::Prepare(fast_key);
    }
    AudioPlayer fast;
    fast.source_now = {Psram, 0, static_cast<uint32_t>(source.size()), source.data()};
    fast.fft_mode_wait = LOOP_FWD;
    fast.pitch = 1.7f;
    fast.Start_fft();
    audio_update_cycle = 100;
    const uint32_t hits = FftAttackCache::Hits();
    fast.Update_fft();
    assert(fast.fft_attack_cached && fast.fft_core.GeneratedHops() > 0);
    assert(fast.envelope.calls == AUDIO_BLOCK_SAMPLES && FftAttackCache::Hits() == hits + 1);
    FftAttackCache::Invalidate();
    // Preparing the first frame must not consume source samples or alter the subsequent PCM.
    for (uint8_t mode = 0; mode < 6; ++mode)
    {
        for (float pitch : {0.5f, 1.0f, 1.7f, 2.24492410f})
        {
            FftPitchCore staged;
            FftPitchCore immediate;
            staged.StartNote(source.data(), source.size(), pitch, 0.5f);
            immediate.StartNote(source.data(), source.size(), pitch, 0.5f);
            staged.SetMode(AudioPlayer::Fft_mode(mode));
            immediate.SetMode(AudioPlayer::Fft_mode(mode));
            const uint32_t first = staged.Transport();
            for (unsigned step = 0; step < 6; ++step)
            {
                assert(!staged.PrepareStartStep());
                assert(staged.Transport() == first && staged.GeneratedHops() == 0);
            }
            assert(staged.PrepareStartStep());
            for (unsigned i = 0; i < 5000; ++i)
            {
                assert(staged.Render() == immediate.Render());
            }
        }
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
            assert(player.transfers < 1200); // Two analysis bands approximately double bounded cache traffic; still far below per-sample transfers.
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
        struct RecordedSource { const int16_t *samples; unsigned *written; } recorded = {source.data(), &written};
        reference.SetSampleReader([](void *context, uint32_t position) -> int16_t
        {
            const auto &recorded = *static_cast<RecordedSource *>(context);
            return position < *recorded.written ? recorded.samples[position] : 0;
        }, &recorded);
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
    recorder.Q_sample = AudioPlayer::Live_fft_margin - 1;
    assert(loop.Prepare_fft_live() && !loop.fft_live_waiting);
    // Exercise the production render wrapper: queued FFT starts must preserve ADSR attacks.
    AudioPlayer chord[2];
    unsigned next_identity = 0;
    for (auto &voice : chord)
    {
        voice.identity = next_identity++;
        voice.source_now = {Psram, 0, static_cast<uint32_t>(source.size()), source.data()};
        voice.fft_mode_wait = LOOP_FWD;
        voice.Start_fft();
    }
    for (unsigned cycle = 0; cycle < 10; ++cycle)
    {
        audio_update_cycle = cycle;
        for (auto &voice : chord)
        {
            voice.Update_fft();
        }
        for (unsigned voice = 0; voice < 2; ++voice)
        {
            assert((chord[voice].fft_core.GeneratedHops() > 0) == (cycle >= 6u + voice));
            assert((chord[voice].envelope.calls > 0) == (cycle >= 6u + voice));
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
    recorder.Q_sample = AudioPlayer::Live_fft_margin - 1;
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


    // Interval counters survive note restarts and distinguish unavailable input from preparation.
    AudioPlayer diagnostic;
    Recorder diagnostic_recorder;
    std::vector<int16_t> diagnostic_source(LS_buffer_dim, 30000);
    diagnostic.LS_flag = true;
    diagnostic.Live_Sampler = &diagnostic_recorder;
    diagnostic.FIFO = diagnostic_source.data();
    diagnostic_recorder.Q_sample = 1023;
    assert(AudioPlayer::Read_fft_source(&diagnostic, 1024) == 0);
    assert(diagnostic.fft_diagnostics.unrecorded_reads == 1);
    assert(diagnostic.fft_diagnostics.near_writer_reads == 1);
    assert(diagnostic.fft_diagnostics.read_age_min == 0);
    diagnostic.Start_fft();
    diagnostic.Update_fft();
    assert(diagnostic.fft_diagnostics.starts == 1);
    assert(diagnostic.fft_diagnostics.source_waits == 1);
    diagnostic.Start_fft();
    assert(diagnostic.fft_diagnostics.starts == 2);

    FftPitchCore metered, reference_metered;
    metered.StartNote(diagnostic_source.data(), diagnostic_source.size(), 1.0f, 2.0f);
    reference_metered.StartNote(diagnostic_source.data(), diagnostic_source.size(), 1.0f, 2.0f);
    for (unsigned sample = 0; sample < 1024; ++sample)
    {
        assert(metered.Render() == reference_metered.Render());
    }
    const auto interval = metered.TakeDiagnostics();
    assert(interval.renders == 1024 && interval.clips > 0 && interval.peak > 32767);
    assert(interval.nonfinite == 0 && interval.resets == 1);
    assert(metered.TakeDiagnostics().renders == 0);
    // Reading/resetting diagnostic counters must not reset overlap or phase history.
    for (unsigned sample = 0; sample < 1024; ++sample)
    {
        assert(metered.Render() == reference_metered.Render());
    }

    for (unsigned sample = 0; sample < diagnostic_source.size(); ++sample)
    {
        diagnostic_source[sample] = static_cast<int16_t>(12000 * std::sin(sample * 0.007) + 5000 * std::sin(sample * 0.12));
    }
    // Dual-band shared scratch must preserve independent histories across player blocks.
    constexpr unsigned comparison_samples = 16384;
    std::vector<int16_t> expected[2];
    for (unsigned voice = 0; voice < 2; ++voice)
    {
        FftPitchCore isolated;
        isolated.StartNote(diagnostic_source.data(), diagnostic_source.size(), voice == 0 ? 0.5f : 2.0f, 1.0f);
        while (!isolated.PrepareStartStep()) {}
        expected[voice].resize(comparison_samples);
        for (auto &value : expected[voice]) { value = isolated.Render(); }
    }
    FftPitchCore interleaved[2];
    bool prepared[2] = {};
    for (unsigned voice = 0; voice < 2; ++voice)
    {
        interleaved[voice].StartNote(diagnostic_source.data(), diagnostic_source.size(), voice == 0 ? 0.5f : 2.0f, 1.0f);
    }
    while (!prepared[0] || !prepared[1])
    {
        for (unsigned voice = 0; voice < 2; ++voice)
        {
            if (!prepared[voice]) { prepared[voice] = interleaved[voice].PrepareStartStep(); }
        }
    }
    for (unsigned offset = 0; offset < comparison_samples; offset += AUDIO_BLOCK_SAMPLES)
    {
        for (unsigned voice = 0; voice < 2; ++voice)
        {
            for (unsigned sample = 0; sample < AUDIO_BLOCK_SAMPLES; ++sample)
            {
                assert(interleaved[voice].Render() == expected[voice][offset + sample]);
            }
        }
    }

    // Live slot 1 is audible again and still advances its envelope and transport.
    AudioPlayer second_live;
    Recorder mute_recorder;
    mute_recorder.first_write_flag = false;
    mute_recorder.Q_sample = 7000;
    second_live.identity = 1;
    second_live.LS_flag = true;
    second_live.Live_Sampler = &mute_recorder;
    second_live.FIFO = diagnostic_source.data();
    second_live.pitch = 1.33484f;
    LS_XY_lock = false;
    second_live.Start_fft();
    bool second_slot_audible = false;
    for (unsigned cycle = 0; cycle < 24; ++cycle)
    {
        ++audio_update_cycle;
        second_live.Update_fft();
        for (const auto &channel : second_live.sent)
        {
            for (int16_t value : channel.data) { second_slot_audible |= value != 0; }
        }
    }
    assert(second_slot_audible);
    assert(second_live.fft_core.GeneratedHops() > 0);
    assert(second_live.envelope.calls > 0);
    assert(second_live.fft_diagnostics.gain_peak > 0);
    assert(second_live.state == AudioPlayer::PlayerState::Playing);
    second_live.powered = false;
    second_live.envelope.phase = 3;
    second_live.envelope.gain = 0;
    ++audio_update_cycle;
    second_live.Update_fft();
    assert(second_live.state == AudioPlayer::PlayerState::Idle);

    // Loading another band's input must not overwrite a partially filled frame.
    FftSpectralFrame frame_a, frame_b, isolated_a, isolated_b;
    frame_a.Reset(); frame_b.Reset(); isolated_a.Reset(); isolated_b.Reset();
    for (unsigned i = 0; i < FftSpectralFrame::Size; ++i)
    {
        const float a = 12000.0f * std::sin(i * 0.031f);
        const float b = 7000.0f * std::sin(i * 0.127f);
        isolated_a.Input(i, a);
        frame_a.Input(i, a);
        frame_b.Input(i, b);
        isolated_b.Input(i, b);
    }
    isolated_a.Analyze(false);
    isolated_b.Analyze(false);
    frame_b.Analyze(false);
    frame_a.Analyze(false);
    isolated_a.Generate(1.25992f, FftSpectralFrame::Band::Full);
    isolated_b.Generate(1.33484f, FftSpectralFrame::Band::Full);
    frame_a.Generate(1.25992f, FftSpectralFrame::Band::Full);
    frame_b.Generate(1.33484f, FftSpectralFrame::Band::Full);
    for (unsigned i = 0; i < FftSpectralFrame::Size; ++i)
    {
        assert(frame_a.Take() == isolated_a.Take());
        assert(frame_b.Take() == isolated_b.Take());
    }

    std::puts("PASS: actual Lilla FFT adapter: Flash/PSRAM, all modes, bounded cache reads, live startup, circular overwrite, first-fill guard and stop drain at pitch extremes.");
}
