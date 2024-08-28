#ifndef TWI_CONTROL_H
#define TWI_CONTROL_H

#include <avr/io.h>

void twi_init(void);
void twi_start(void);
void twi_stop(void);
void twi_write(uint8_t data);

#endif
