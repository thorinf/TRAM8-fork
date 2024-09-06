#include "pin_control.h"

#include <avr/eeprom.h>

void pin_initialize(void) {
    GATE_DDR_B |= (1 << GATE_PIN_0);  // Set PB0 as output
    GATE_DDR_D |= 0xFE;               // Set PD1 to PD7 as outputs

    DDRC |= (1 << LED_PIN) | (1 << BUTTON_PIN);
    led_off();  // Turn off LED

    while (!eeprom_is_ready());
    if (eeprom_read_byte(EEPROM_BUTTON_FIX) == 0xAA) {
        BUTTON_DDR &= ~(1 << BUTTON_PIN);
    }

    CONTROL_DDR |= (1 << LDAC_PIN) | (1 << CLR_PIN);
    LDAC_PORT |= (1 << LDAC_PIN);  // Set LDAC high to hold the DAC output (active-low signal, high means no update)
    CLR_PORT |= (1 << CLR_PIN);    // Set CLR high to avoid clearing the DAC (active-low signal, high means no clear)
}

void gate_set(uint8_t gateIndex, uint8_t state) {
    uint8_t pin = (gateIndex == 0) ? GATE_PIN_0 : GATE_PIN_1 + gateIndex - 1;
    volatile uint8_t *port = (gateIndex == 0) ? &GATE_PORT_B : &GATE_PORT_D;
    if (state) {
        *port |= (1 << pin);
    } else {
        *port &= ~(1 << pin);
    }
}

void gate_set_multiple(uint8_t gateMask, uint8_t state) {
    uint8_t portBMask = (gateMask & 0x01) << GATE_PIN_0;
    uint8_t portDMask = ((gateMask >> 1) & 0x7F) << GATE_PIN_1;

    if (state) {
        GATE_PORT_B |= portBMask;
        GATE_PORT_D |= portDMask;
    } else {
        GATE_PORT_B &= ~portBMask;
        GATE_PORT_D &= ~portDMask;
    }
}