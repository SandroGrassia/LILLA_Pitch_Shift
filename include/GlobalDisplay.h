/*
   LILLA Audio Sampler
   Author: Sandro Grassia (info@lillasampler.it)
   www.lillasampler.it
*/

#pragma once

#include <Adafruit_GFX.h>
#include <ILI9341_t3n.h>

extern ILI9341_t3n tft;
// The waveform borrows fixed pixel storage, independent of heap allocation.
class WaveformCanvas final : public GFXcanvas16
{
public:
    WaveformCanvas(uint16_t width, uint16_t height, uint16_t *pixels)
        : GFXcanvas16(width, height, false)
    {
        buffer = pixels;
    }
};

extern WaveformCanvas canvas;
