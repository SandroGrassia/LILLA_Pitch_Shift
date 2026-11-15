/*
 * LILLA Audio Sampler
 * Author: Sandro Grassia, info@lillasampler.it
 */
#pragma once

// FFT ceiling: +12 semitones of base transposition plus +2 semitones of bend.
static constexpr float MAX_PITCH_FLASH = 2.24492410f;
static constexpr float MAX_PITCH_PSRAM = 2.24492410f; // Cached sample files.
static constexpr float MAX_PITCH_LIVE = 2.24492410f; // Shared FFT ceiling, including Live Sampler.
static constexpr float MAX_PITCH_WAVETABLE = 2.24492410f;

float Sample_pitch_limit(bool cached); // Return the sample pitch ceiling for the selected storage.

float Playback_pitch_limit(bool wavetable, bool cached, bool live); // Return the pitch ceiling for wavetable, cached or live playback.
