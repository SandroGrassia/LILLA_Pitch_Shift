#pragma once

// Four-pole Butterworth at 0.45/max(1,pitch) cycles per intermediate sample.
// Process every intermediate sample in order, before resampling. Pitch is 0.25..4.
class WsolaAntialias
{
public:
    void SetPitch(float pitch);
    void Reset(float sample); // Initialize at DC equilibrium, adopting the current target coefficients.
    float Process(float sample);

private:
    struct Section
    {
        float b0 = 1.0f;
        float a1 = 0.0f;
        float a2 = 0.0f;
        float target_b0 = 1.0f;
        float target_a1 = 0.0f;
        float target_a2 = 0.0f;
        float z1 = 0.0f;
        float z2 = 0.0f;
    };
    Section sections[2];
    float configured_pitch = 0.0f;
};
