#include "pin_control.h"

#include <avr/eeprom.h>

void pin_initialize(void) {
    GATE_DDR_B |= (1 << GATE_PIN_0);  // Set PB0 as output
    GATE_DDR_D |= 0xFE;               // Set PD1 to PD7 as outputs

    DDRC |= (1 << LED_PIN);
    led_off();

    // This is to match original codebase, true case ought to be default
    while (!eeprom_is_ready());
    if (eeprom_read_byte((uint8_t *)EEPROM_BUTTON_FIX_ADDR) == 0xAA) {
        BUTTON_DDR &= ~(1 << BUTTON_PIN);
    } else {
        BUTTON_DDR |= ~(1 << BUTTON_PIN);
    }

    CONTROL_DDR |= (1 << LDAC_PIN) | (1 << CLR_PIN);
    LDAC_PORT |= (1 << LDAC_PIN);
    CLR_PORT |= (1 << CLR_PIN);
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