# Third-party notices for LILLA PitchShift

This inventory accompanies the `lilla_fft_r2` firmware and its sources.
The original LILLA code is covered by [LICENSING.md](LICENSING.md); the
pre-existing third-party material below retains its own terms.
Complete copied notices are in `LICENSES/`. Summaries in this table do not
replace those texts.

## Direct dependencies

| Component and inspected version | Terms and attribution | Included notices |
| --- | --- | --- |
| [Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library), 1.13.0 | BSD-2-Clause; Adafruit Industries. | [Adafruit-GFX.txt](LICENSES/Adafruit-GFX.txt) |
| [Adafruit MCP23017](https://github.com/adafruit/Adafruit-MCP23017-Arduino-Library), 2.3.2 | BSD-3-Clause; Adafruit Industries. | [Adafruit-MCP23017.txt](LICENSES/Adafruit-MCP23017.txt) |
| [Adafruit BusIO](https://github.com/adafruit/Adafruit_BusIO), 1.17.4 | MIT; Adafruit Industries. | [Adafruit-BusIO.txt](LICENSES/Adafruit-BusIO.txt) |
| [ILI9341_t3n](https://github.com/KurtE/ILI9341_t3n/tree/f1bfb81825), revision `f1bfb81825` | MIT declarations and BSD-2-Clause graphics portions. Credits include Limor Fried/Adafruit, Kurt Eckhardt, Tim Trzepacz/SoftEgg and Frank Boesing. | [ILI9341_t3n.txt](LICENSES/ILI9341_t3n.txt) |
| [dr_libs](https://github.com/mackron/dr_libs/tree/dfe8377631000664666519fdb83da193fd8037f4), revision `dfe8377631000664666519fdb83da193fd8037f4` | David Reid; MIT-0 selected from the offered MIT-0/Unlicense alternatives. `dr_mp3.h` includes minimp3 with its own public-domain/CC0 notice. | [dr_libs.txt](LICENSES/dr_libs.txt), [CC0-1.0.txt](LICENSES/CC0-1.0.txt) |

The installed dr_libs package has a root copyright notice dated 2020 and a
dr_mp3 license notice dated 2023. Both are retained verbatim. The root dual
license does not replace the embedded minimp3 notice.

## Teensy framework and runtime

These framework components were inspected in
`platformio/framework-arduinoteensy@1.162.0` (Teensyduino 1.62), selected through
`teensy@5.1.0` for Teensy 4.1. Notices are collected from the relevant component
source directories, excluding examples, tests and documentation. A collected
notice may cover an optional source file not linked into the final HEX.

| Component | Terms and attribution | Included notices |
| --- | --- | --- |
| [Teensy 4 core](https://github.com/PaulStoffregen/cores/tree/master/teensy4) | Mixed: PJRC permissive terms with a target-selection condition, MIT-like terms, LGPL-2.1-or-later, BSD and Apache-2.0. Preserve each file's notice. See the unresolved combination question in [LICENSING.md](LICENSING.md#compatibility-of-the-firmware-combination). | [Teensy-core.txt](LICENSES/Teensy-core.txt), [LGPL-2.1.txt](LICENSES/LGPL-2.1.txt) |
| ARM CMSIS / CMSIS-DSP, installed headers identify DSP 1.5.1 | Apache-2.0; ARM Limited and affiliates. Includes `arm_math.h`, `arm_const_structs.h`, tables and Cortex-M headers. The ELF contains `arm_cfft_f32` and `arm_cfft_sR_f32_len1024`. | ARM notices in [Teensy-core.txt](LICENSES/Teensy-core.txt), full [Apache-2.0.txt](LICENSES/Apache-2.0.txt) |
| [Teensy Audio](https://github.com/PaulStoffregen/Audio), library version 1.3 | Predominantly MIT-like permissions with copyright and funding notices; contributors include Paul Stoffregen and others named in the files. Audio memcpy helpers carry the PJRC target-selection condition. | [Teensy-Audio.txt](LICENSES/Teensy-Audio.txt) |
| [SPI](https://github.com/PaulStoffregen/SPI), 1.0 | GPL-2.0 or LGPL-2.1; the LGPL alternative is selected. Cristian Maglie, Paul Stoffregen and Matthijs Kooijman. | [Teensy-SPI.txt](LICENSES/Teensy-SPI.txt), [LGPL-2.1.txt](LICENSES/LGPL-2.1.txt), [GPL-2.0.txt](LICENSES/GPL-2.0.txt) |
| [Wire](https://github.com/PaulStoffregen/Wire), 1.0 | Wrapper/AVR sources have LGPL-2.1-or-later notices; Teensy implementation headers have permissive PJRC notices including funding attribution. | [Teensy-Wire.txt](LICENSES/Teensy-Wire.txt), [LGPL-2.1.txt](LICENSES/LGPL-2.1.txt) |
| [SD wrapper](https://github.com/PaulStoffregen/SD), 2.0.0 | MIT-like PJRC notice; this is the Teensy SdFat wrapper. | [Teensy-SD.txt](LICENSES/Teensy-SD.txt) |
| [SdFat Teensy fork](https://github.com/PaulStoffregen/SdFat), 2.1.2 | Main library: MIT, Bill Greiman. The installed package also contains DigitalIO/SoftSPI headers with GPL-3.0-or-later notices; these are retained rather than labeling the entire package MIT. | [SdFat.txt](LICENSES/SdFat.txt), [GPLv3 text](LICENSE) |
| [SerialFlash](https://github.com/PaulStoffregen/SerialFlash), bundled revision | MIT-like PJRC notice including funding attribution. | [SerialFlash.txt](LICENSES/SerialFlash.txt) |
| [Arduino MIDI Library](https://github.com/FortySevenEffects/arduino_midi_library), 5.0.2 | MIT; Francois Best and contributors named in the files. | [MIDI.txt](LICENSES/MIDI.txt) |
| GCC/libstdc++/libgcc runtime, toolchain package `1.150201.0`, GCC 15.2.1 | GPLv3-or-later with GCC Runtime Library Exception 3.1 where granted by the relevant runtime files. Compiler use alone does not impose GPL on application code. | [GCC-runtime.txt](LICENSES/GCC-runtime.txt), [GCC-exception-3.1.txt](LICENSES/GCC-exception-3.1.txt), [GPLv3 text](LICENSE) |
| Newlib, installed headers identify 4.5.0 | Multiple upstream copyright notices and licenses. The complete `COPYING.NEWLIB` from the official 4.5.0.20241231 source release is included. It covers the upstream package, not an assertion that every listed component is linked. | [Newlib.txt](LICENSES/Newlib.txt) |

The framework's current upstream URLs identify the projects. The installed
package version and the source-file hashes in [manifest.json](LICENSES/manifest.json)
identify the inspected material; an upstream `master` branch is not a pinned
replacement for that material. The Newlib notice source is the official release
matching the installed 4.5.0 family; it does not establish the absence of
toolchain-vendor patches. Preserve any additional vendor notices when supplying
the toolchain or its corresponding sources.

## Third-party material inside the repository

The following local components retain Paul Stoffregen/PJRC audio-library
copyright, permission and funding notices:

- `AmpliOutMuteIn`, `AudioGain`, `AudioPeakDetector`, `AudioVCF`.
- `Mixer_2x1`, `Router_16x3`, `WaveLFO`, `WaveSine`.
- `output_noiseshaped_pwm`.

These attributions cover the pre-existing material in the component headers
and associated implementations. `LillaFRAM_2x512` derives from
`FRAM_MB85RC_I2C` by SOSAndroid.fr (E. Ha.) and retains BSD-3-Clause terms.
`MidiReader` and `MidiOut` retain their credits to Francois Best and the linked
upstream discussion. A credit is recorded as an attribution, not treated as a
separate license grant.

The exact notices and source locations are reproduced in
[LILLA-derived.txt](LICENSES/LILLA-derived.txt). Existing source notices remain
authoritative and must stay with the source when it is redistributed.

## License text provenance

The GPLv3, LGPLv2.1 and GCC exception texts were copied from the GCC project's
`releases/gcc-15.2.0` license files. Apache-2.0 was copied from the Apache Software
Foundation. CC0-1.0 was copied from the SPDX license text catalog. Newlib notices
were extracted from the official Sourceware release archive. The preserved GPLv2
text is the repository's previous `LICENSE` file. Other notice files contain
verbatim source comments and package license files, grouped only to avoid
repeating identical notices.

Origins, text hashes and contributing source-file hashes are recorded in
[LICENSES/manifest.json](LICENSES/manifest.json). These notices accompany the
source-delivery obligations described in [LICENSING.md](LICENSING.md#distributing-firmware).
