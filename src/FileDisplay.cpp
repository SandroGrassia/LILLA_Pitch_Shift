#include "FileDisplay.h"

// From WHOLE_LILLA: src/main.cpp and DisplayPrimitives, Sandro Grassia.
// Write-only SPI1 display; MISO 39 belongs to the MCP23S17 devices.
FileDisplay::FileDisplay() : tft(38, 29, 30, 26, 27, 255)
{
}

void FileDisplay::Begin()
{
    tft.begin();
    tft.setRotation(1);
    tft.setTextWrap(false);
    tft.fillScreen(ILI9341_BLACK);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.setCursor(8, 12);
    tft.print("FFT region 0.7.10");
    tft.setTextSize(1);
    tft.setCursor(8, 42);
    tft.print("R1 VALUE: file  SELECT: cross-mix");
}

void FileDisplay::Show(const char *selected, const char *loaded, const char *status)
{
    // Clear only the changing text area, following Lilla's Cancel_text pattern.
    tft.fillRect(8, 58, 304, 98, ILI9341_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(8, 58);
    tft.print("SELECTED");
    tft.setCursor(8, 70);
    tft.setTextColor(ILI9341_YELLOW);
    tft.setTextWrap(true);
    tft.print(selected);
    tft.setTextWrap(false);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(8, 100);
    tft.print("LOADED");
    tft.setCursor(8, 112);
    tft.setTextWrap(true);
    tft.print(loaded);
    tft.setTextWrap(false);
    tft.setCursor(8, 142);
    tft.print(status);
}

void FileDisplay::ShowRegion(uint32_t first, uint32_t last, uint32_t total, uint32_t step, bool automatic, bool slice, uint32_t mix)
{
    tft.fillRect(8, 160, 304, 80, ILI9341_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(8, 160);
    tft.print("FROM ");
    tft.print(first);
    tft.print("  TO ");
    tft.print(last);
    tft.setCursor(8, 175);
    tft.print("SAMPLES ");
    tft.print(last - first + 1);
    tft.print("  ");
    tft.print((last - first + 1) / 44100.0f, 3);
    tft.print(" s");
    tft.setCursor(8, 190);
    tft.print("STEP ");
    tft.print(step);
    tft.print(automatic ? " (1/16)" : " samples");
    tft.setCursor(8, 205);
    tft.print(slice ? "SLICE: FROM moves region" : "TO: independent endpoints");
    tft.setCursor(8, 220);
    tft.print("XMIX ");
    tft.print(mix);
    tft.print(" S / ");
    tft.print(mix / 44.1f, 2);
    tft.print(" ms");
    tft.drawRect(8, 232, 304, 6, ILI9341_WHITE);
    const uint32_t left = static_cast<uint64_t>(first) * 302u / total;
    const uint32_t right = static_cast<uint64_t>(last + 1u) * 302u / total;
    tft.fillRect(9 + left, 233, right > left ? right - left : 1, 4, ILI9341_YELLOW);
}

void FileDisplay::ShowMode(const char *name, bool once, bool edit_mode)
{
    tft.fillRect(8, 31, 304, 9, ILI9341_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_YELLOW);
    tft.setCursor(8, 31);
    tft.print(name);
    if (once)
    {
        tft.print(" (XMIX ignored)");
    }
    tft.fillRect(8, 42, 304, 9, ILI9341_BLACK);
    tft.setCursor(8, 42);
    tft.setTextColor(ILI9341_WHITE);
    tft.print(edit_mode ? "VALUE: file SELECT: MODE [press: XMIX]" : "VALUE: file SELECT: XMIX [press: MODE]");
}
