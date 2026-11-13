#pragma once
#include <ILI9341_t3n.h>

// Minimal extraction of Lilla's display setup and text-clearing approach.
class FileDisplay
{
public:
    FileDisplay();
    void Begin();
    void Show(const char *selected, const char *loaded, const char *status);
private:
    ILI9341_t3n tft;
};
