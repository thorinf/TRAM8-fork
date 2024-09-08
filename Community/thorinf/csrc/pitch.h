#ifndef PITCH_LOOKUP_H
#define PITCH_LOOKUP_H

#include <avr/io.h>

#define PITCH_SIZE 61

// enum Pitch {
//     C0 = 0, Cs0, D0, Ds0, E0, F0, Fs0, G0, Gs0, A0, As0, B0,
//     C1, Cs1, D1, Ds1, E1, F1, Fs1, G1, Gs1, A1, As1, B1,
//     C2, Cs2, D2, Ds2, E2, F2, Fs2, G2, Gs2, A2, As2, B2,
//     C3, Cs3, D3, Ds3, E3, F3, Fs3, G3, Gs3, A3, As3, B3,
//     C4, Cs4, D4, Ds4, E4, F4, Fs4, G4, Gs4, A4, As4, B4,
//     C5, Cs5, D5, Ds5, E5, F5, Fs5, G5, Gs5, A5, As5, B5,
//     C6, Cs6, D6, Ds6, E6, F6, Fs6, G6, Gs6, A6, As6, B6,
//     C7, Cs7, D7, Ds7, E7, F7, Fs7, G7, Gs7, A7, As7, B7,
//     C8, Cs8, D8, Ds8, E8, F8, Fs8, G8, Gs8, A8, As8, B8,
//     C9, Cs9, D9, Ds9, E9, F9, Fs9, G9, Gs9, A9, As9, B9,
//     C10, Cs10, D10, Ds10, E10, F10, Fs10, G10
// };

// Pitch lookup array definition
static const uint16_t pitch_lookup[61] = {
    0x0000, 0x0440, 0x0880, 0x0CD0, 0x1110, 0x1550, 0x19A0, 0x1DE0, 0x2220, 0x2660, 0x2AA0, 0x2EF0,  // C-2
    0x3330, 0x3770, 0x3BC0, 0x4000, 0x4440, 0x4880, 0x4CC0, 0x5110, 0x5550, 0x5990, 0x5DE0, 0x6220,  // C-1
    0x6660, 0x6AA0, 0x6EE0, 0x7330, 0x7770, 0x7BB0, 0x8000, 0x8440, 0x8880, 0x8CC0, 0x9100, 0x9550,  // C0
    0x9990, 0x9DD0, 0xA220, 0xA660, 0xAAA0, 0xAEE0, 0xB320, 0xB770, 0xBBB0, 0xBFF0, 0xC440, 0xC880,  // C1
    0xCCC0, 0xD100, 0xD550, 0xD990, 0xDDD0, 0xE210, 0xE660, 0xEAA0, 0xEEE0, 0xF320, 0xF760, 0xFBB0,  // C2
    0xFFF0                                                                                           // C3
};

#endif
