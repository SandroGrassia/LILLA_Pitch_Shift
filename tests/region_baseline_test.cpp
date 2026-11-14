#include "FftPitchCore.h"
#include "BaselineFftPitchCore.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

int main()
{
    std::vector<int16_t> source(4099);
    for (unsigned i = 0; i < source.size(); ++i)
    {
        source[i] = static_cast<int16_t>(12000 * std::sin(i * 0.19) + 4000 * std::sin(i * 0.73));
    }
    for (float pitch : {0.25f, 0.7f, 1.0f, 1.5f, 3.0f, 4.0f})
    {
        FftPitchCore current;
        BaselineFftPitchCore baseline;
        current.StartNote(source.data(), source.size(), pitch, 0.5f);
        baseline.StartNote(source.data(), source.size(), pitch, 0.5f);
        for (unsigned i = 0; i < 16000; ++i)
        {
            if (i == 3000)
            {
                current.SetPitch(1.0f);
                baseline.SetPitch(1.0f);
            }
            if (i == 9000)
            {
                current.SetPitch(2.7f);
                baseline.SetPitch(2.7f);
            }
            if (i == 15000)
            {
                current.SetRunning(false);
                baseline.SetRunning(false);
            }
            assert(current.Render() == baseline.Render());
        }
    }
    std::puts("PASS: full-file audio remains sample-identical to committed 0.6.8, including bends and release.");
}
