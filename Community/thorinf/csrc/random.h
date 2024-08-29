#ifndef RANDOM_H
#define RANDOM_H

#include <avr/io.h>

uint16_t updateLfsr(uint16_t *lfsr);
uint16_t updateLfsrAlt(uint16_t *lfsr);

#endif