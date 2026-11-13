#include "WsolaCore.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <vector>

int main(int argc, char **argv)
{
    if (argc != 7)
    {
        return 1;
    }
    std::ifstream input(argv[1], std::ios::binary | std::ios::ate);
    const auto bytes = input.tellg();
    if (!input || bytes < 4 || bytes % 2 != 0)
    {
        return 2;
    }
    const float pitch = std::strtof(argv[3], nullptr);
    const unsigned window = std::strtoul(argv[4], nullptr, 10);
    const unsigned overlap = std::strtoul(argv[5], nullptr, 10);
    const unsigned search = std::strtoul(argv[6], nullptr, 10);
    if (pitch < 0.25f || pitch > 4.0f || window < 220 || window > WsolaCore::MaxWindow || window % 2 != 0 || (overlap != 50 && overlap != 75) || search > WsolaCore::MaxSearch)
    {
        return 3;
    }
    std::vector<int16_t> source(static_cast<size_t>(bytes) / 2);
    input.seekg(0);
    input.read(reinterpret_cast<char *>(source.data()), bytes);
    if (!input)
    {
        return 4;
    }
    auto core = std::make_unique<WsolaCore>();
    core->SetWindow(window);
    core->SetOverlap(overlap);
    core->SetSearch(search);
    core->SetAntialias(true);
    core->StartNote(source.data(), static_cast<uint32_t>(source.size()), pitch, 0.5f);
    std::ofstream output(argv[2], std::ios::binary);
    for (size_t i = 0; i < source.size(); ++i)
    {
        const int16_t sample = core->Render();
        output.write(reinterpret_cast<const char *>(&sample), sizeof(sample));
    }
    std::printf("hops=%u transport=%u\n", static_cast<unsigned>(core->GeneratedHops()), static_cast<unsigned>(core->Transport()));
    return output && core->Transport() == 0 ? 0 : 5;
}
