#ifndef PIN_CONTROL_H
#define PIN_CONTROL_H

#include "hardware_config.h"

#define led_on() (LED_PORT &= ~(1 << LED_PIN))
#define led_off() (LED_PORT |= (1 << LED_PIN))

void pin_initialize(void);
void gate_set(uint8_t gateIndex, uint8_t state);
void gate_set_multiple(uint8_t gateMask, uint8_t state);

#endif