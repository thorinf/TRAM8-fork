#ifndef PIN_CONTROL_H
#define PIN_CONTROL_H

#include "hardware_config.h"

void pin_initialize(void);

static inline uint8_t read_button(void) { return BUTTON_PIN_REG & (1 << BUTTON_PIN); }

static inline void led_on(void) { LED_PORT &= ~(1 << LED_PIN); }

static inline void led_off(void) { LED_PORT |= (1 << LED_PIN); }

static inline void gate_set(uint8_t gateIndex, uint8_t state) {
    static const uint8_t gatePortMasks[NUM_GATES] = {
        (1 << GATE_PIN_0),       (1 << (GATE_PIN_1 + 0)), (1 << (GATE_PIN_1 + 1)), (1 << (GATE_PIN_1 + 2)),
        (1 << (GATE_PIN_1 + 3)), (1 << (GATE_PIN_1 + 4)), (1 << (GATE_PIN_1 + 5)), (1 << (GATE_PIN_1 + 6))};

    static volatile uint8_t *const gatePorts[NUM_GATES] = {&GATE_PORT_B, &GATE_PORT_D, &GATE_PORT_D, &GATE_PORT_D,
                                                           &GATE_PORT_D, &GATE_PORT_D, &GATE_PORT_D, &GATE_PORT_D};

    volatile uint8_t *port = gatePorts[gateIndex];
    uint8_t mask = gatePortMasks[gateIndex];

    if (state) {
        *port |= mask;
    } else {
        *port &= ~mask;
    }
}

void gate_set_multiple(uint8_t gateMask, uint8_t state);

#endif