#include "max5825_control.h"

#include "twi_control.h"

#define MAX5825_ADDR 0x20
#define MAX5825_REG_REF 0x20
#define MAX5825_REG_CODEn_LOADall 0xA0
#define MAX5825_REG_CODEn_LOADn 0xB0

void max5825_init() {
    twi_start();
    twi_write(MAX5825_ADDR);
    twi_write(MAX5825_REG_REF | 0b101);  // Setup command for reference voltage
    twi_write(0x00);
    twi_write(0x00);
    twi_stop();

    twi_start();
    twi_write(MAX5825_ADDR);
    twi_write(MAX5825_REG_CODEn_LOADall);
    twi_write(0x00);
    twi_write(0x00);
    twi_stop();
}

void max5825_write(uint8_t channel, uint16_t value) {
    twi_start();
    twi_write(MAX5825_ADDR);
    twi_write(MAX5825_REG_CODEn_LOADn | (channel & 0x0F));
    twi_write((uint8_t)(value >> 8));    // Upper 8 bits (MSB) of the value
    twi_write((uint8_t)(value & 0xF0));  // Lower 4 bits (LSB) shifted into the upper 4 bits
    twi_stop();
}
