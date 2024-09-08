#ifndef MIDIMAP_H
#define MIDIMAP_H

#include <avr/io.h>

#include "hardware_config.h"

// MIDI Map Types
#define NUM_MIDIMAP_TYPES 6
#define MIDIMAP_VELOCITY 0
#define MIDIMAP_CC 1
#define MIDIMAP_PITCH 2
#define MIDIMAP_PITCH_SAH 3
#define MIDIMAP_RANDSEQ 4
#define MIDIMAP_RANDSEQ_SAH 5

// MIDI Map Struct Definition
typedef struct {
    uint8_t mapType;
    uint8_t gateCommand;
    uint8_t gateValue;
    uint8_t cvCommand1;
    uint8_t cvValue1;
    uint8_t cvCommand2;
    uint8_t cvValue2;
} MIDIMapEntry;

// MIDI mapping for velocity (Original Tram8)
MIDIMapEntry midi_map_velo[NUM_GATES] = {
    {MIDIMAP_VELOCITY, 0x90, 24, 0, 0, 0, 0},  // Gate C0
    {MIDIMAP_VELOCITY, 0x90, 25, 0, 0, 0, 0},  // Gate C#0
    {MIDIMAP_VELOCITY, 0x90, 26, 0, 0, 0, 0},  // Gate D0
    {MIDIMAP_VELOCITY, 0x90, 27, 0, 0, 0, 0},  // Gate D#0
    {MIDIMAP_VELOCITY, 0x90, 28, 0, 0, 0, 0},  // Gate E0
    {MIDIMAP_VELOCITY, 0x90, 29, 0, 0, 0, 0},  // Gate F0
    {MIDIMAP_VELOCITY, 0x90, 30, 0, 0, 0, 0},  // Gate F#0
    {MIDIMAP_VELOCITY, 0x90, 31, 0, 0, 0, 0}   // Gate G0
};

// MIDI mapping for CC (Original Tram8)
MIDIMapEntry midi_map_cc[NUM_GATES] = {
    {MIDIMAP_VELOCITY, 0x90, 24, 0xB0, 69, 0, 0},  // Gate C0
    {MIDIMAP_VELOCITY, 0x90, 25, 0xB0, 70, 0, 0},  // Gate C#0
    {MIDIMAP_VELOCITY, 0x90, 26, 0xB0, 71, 0, 0},  // Gate D0
    {MIDIMAP_VELOCITY, 0x90, 27, 0xB0, 72, 0, 0},  // Gate D#0
    {MIDIMAP_VELOCITY, 0x90, 28, 0xB0, 73, 0, 0},  // Gate E0
    {MIDIMAP_VELOCITY, 0x90, 29, 0xB0, 74, 0, 0},  // Gate F0
    {MIDIMAP_VELOCITY, 0x90, 30, 0xB0, 75, 0, 0},  // Gate F#0
    {MIDIMAP_VELOCITY, 0x90, 31, 0xB0, 76, 0, 0}   // Gate G0
};

// MIDI mapping for the BeatStep Pro
MIDIMapEntry midi_map_bsp[NUM_GATES] = {
    {MIDIMAP_RANDSEQ_SAH, 0x97, 36, 0x97, 44, 0x97, 45},  // Gate C0, Step G#0, Reset A0
    {MIDIMAP_RANDSEQ_SAH, 0x97, 37, 0x97, 46, 0x97, 47},  // Gate C#0, Step A#0, Reset B0
    {MIDIMAP_RANDSEQ, 0x97, 38, 0x97, 48, 0x97, 49},      // Gate D0, Step C1, Reset C#1
    {MIDIMAP_RANDSEQ, 0x97, 39, 0x97, 50, 0x97, 51},      // Gate D#0, Step D1, Reset D#1
    {MIDIMAP_VELOCITY, 0x97, 40, 0, 0, 0, 0},             // Gate E0
    {MIDIMAP_VELOCITY, 0x97, 41, 0, 0, 0, 0},             // Gate F0
    {MIDIMAP_PITCH, 0x90, 0, 0, 0, 0, 0},                 // Sequencer 1
    {MIDIMAP_PITCH, 0x91, 0, 0, 0, 0, 0},                 // Sequencer 2
};

#endif