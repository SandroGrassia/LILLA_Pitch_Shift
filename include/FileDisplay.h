#pragma once
#include <ILI9341_t3n.h>

// Minimal extraction of Lilla's display setup and text-clearing approach.
class FileDisplay
{
public:
    FileDisplay();
    void Begin();
    void ShowMode(bool once);
    void Show(const char *selected, const char *loaded, const char *status);
    void ShowRegion(uint32_t first, uint32_t last, uint32_t total, uint32_t step, bool automatic, bool slice, uint32_t mix);
private:
    ILI9341_t3n tft;
};
