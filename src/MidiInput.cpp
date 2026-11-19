#include "MidiInput.h"
#include "ExperimentConfig.h"
#include <Arduino.h>
#include <MIDI.h>

// Same DIN transport as Lilla; USB Serial is reserved for diagnostics.
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

void MidiInput::Begin()
{
    static uint8_t extra_rx[512];
    Serial1.addMemoryForRead(extra_rx, sizeof(extra_rx));
    MIDI.begin(ExperimentConfig::MidiChannel);
    MIDI.turnThruOff();
}

uint8_t MidiInput::Collect()
{
    static_assert(midi::DefaultSettings::Use1ByteParsing, "The RX byte budget requires one-byte MIDI parsing.");
    const int available = Serial1.available();
    const int limit = available < 64 ? available : 64;
    const uint32_t started = micros();
    uint8_t count = 0;
    for (int bytes = 0; bytes < limit && count < Capacity; ++bytes)
    {
        if (static_cast<uint32_t>(micros() - started) >= 80u)
        {
            break;
        }
        if (MIDI.read() && MIDI.getChannel() == ExperimentConfig::MidiChannel)
        {
            const uint8_t type = static_cast<uint8_t>(MIDI.getType());
            if (type == midi::NoteOn || type == midi::NoteOff || type == midi::PitchBend || type == midi::ControlChange)
            {
                messages[count++] = {type, MIDI.getData1(), MIDI.getData2()};
            }
        }
    }
    return count;
}
