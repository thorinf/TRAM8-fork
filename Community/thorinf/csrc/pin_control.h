#ifndef PIN_CONTROL_H
#define PIN_CONTROL_H

#include "hardware_config.h"

void pin_initialize(void);
uint8_t read_button(void);
void led_on(void);
void led_off(void);
void gate_set(uint8_t gateIndex, uint8_t state);
void gate_set_multiple(uint8_t gateMask, uint8_t state);

#endif