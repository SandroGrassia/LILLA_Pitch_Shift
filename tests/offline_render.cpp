#include "FftPitchCore.h"
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <vector>

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        return 1;
    }
    std::ifstream input(argv[1], std::ios::binary | std::ios::ate);
    const auto bytes = input.tellg();
    if (!input || bytes < 4 || bytes % 2 != 0)
    {
        return 2;
    }
    std::vector<int16_t> source(static_cast<size_t>(bytes) / 2);
    input.seekg(0);
    input.read(reinterpret_cast<char *>(source.data()), bytes);
    const float pitch = std::strtof(argv[3], nullptr);
    if (!input || pitch < 0.25f || pitch > 4.0f)
    {
        return 3;
    }
    FftPitchCore core;
    core.StartNote(source.data(), static_cast<uint32_t>(source.size()), pitch, 0.5f);
    std::ofstream output(argv[2], std::ios::binary);
    for (size_t i = 0; i < source.size(); ++i)
    {
        const int16_t sample = core.Render();
        output.write(reinterpret_cast<const char *>(&sample), sizeof(sample));
    }
    std::printf("transient_resets=%u\n", static_cast<unsigned>(core.TransientResets()));
    return output ? 0 : 4;
}
