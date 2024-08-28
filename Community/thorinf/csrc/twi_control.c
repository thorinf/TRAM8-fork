#include "twi_control.h"

void twi_init(void) {
    TWSR = 0x00;  // Set prescaler to zero
    TWBR = 12;    // Baud rate set to 400kHz assuming F_CPU is 16MHz and no prescaler
    TWCR = (1 << TWEN);  // Enable TWI
}

void twi_start(void) {
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));  // Wait for TWINT Flag set indicating START condition has been transmitted
}

void twi_stop(void) {
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
    // No while loop here as STOP doesn't require polling TWINT
}

void twi_write(uint8_t data) {
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));  // Wait for TWINT Flag set indicating data has been transmitted
}
