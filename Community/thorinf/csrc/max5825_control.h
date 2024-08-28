#ifndef MAX5825_H
#define MAX5825_H

#include <avr/io.h>

void max5825_init(void);
void max5825_write(uint8_t channel, uint16_t value);

#endif