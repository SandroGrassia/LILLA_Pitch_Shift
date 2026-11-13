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
    tft.print("FFT6 0.6.8");
    tft.setTextSize(1);
    tft.setCursor(8, 42);
    tft.print("Encoder VALUE: choose RAW file");
}

void FileDisplay::Show(const char *selected, const char *loaded, const char *status)
{
    // Clear only the changing text area, following Lilla's Cancel_text pattern.
    tft.fillRect(8, 68, 304, 164, ILI9341_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(8, 68);
    tft.print("SELECTED");
    tft.setCursor(8, 86);
    tft.setTextColor(ILI9341_YELLOW);
    tft.setTextWrap(true);
    tft.print(selected);
    tft.setTextWrap(false);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(8, 126);
    tft.print("LOADED");
    tft.setCursor(8, 144);
    tft.setTextWrap(true);
    tft.print(loaded);
    tft.setTextWrap(false);
    tft.setCursor(8, 194);
    tft.print(status);
}
