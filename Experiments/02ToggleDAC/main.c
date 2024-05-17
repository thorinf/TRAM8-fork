// Define the CPU frequency and MIDI baud rate
#define F_CPU 16000000UL
#define BAUD 31250UL

#include <avr/io.h>
#include <util/delay.h>

// TWI (I2C) parameters, these are predefined in the ATmega8 - here for reference
#define SDA_PIN PC4 // SDA pin
#define SCL_PIN PC5 // SCL pin

// Pins for additional control
#define LDAC_PIN PC2
#define CLR_PIN PC3
#define LDAC_PORT PORTC
#define CLR_PORT PORTC
#define CONTROL_DDR DDRC

// Macros for setting and clearing bits
#define SET_BIT(PORT, PIN) ((PORT) |= (1 << (PIN)))
#define CLEAR_BIT(PORT, PIN) ((PORT) &= ~(1 << (PIN)))

// MAX5825 DAC Definitions
#define MAX5825_ADDR 0x20 // 0010 000W, where W is the write bit
#define MAX5825_REG_REF 0x20
#define MAX5825_REG_CODEn_LOADn 0xB0 // 1011 XXXX, where X encodes the DAC channel
#define MAX5825_REG_CODEn_LOADall 0xA0

// Function prototypes
void setup(void);
void twi_init(void);
void twi_start(void);
void twi_stop(void);
void twi_write(uint8_t data);
void max5825_write(uint8_t channel, uint16_t value);

void setup() {
    // Initialize control pins for DAC
    CONTROL_DDR |= (1 << LDAC_PIN) | (1 << CLR_PIN); // Set LDAC and CLR as outputs 
    // Set LDAC high to hold the DAC output (active-low signal, high means no update)
    LDAC_PORT |= (1 << LDAC_PIN); 

    // Set CLR high to avoid clearing the DAC (active-low signal, high means no clear)
    CLR_PORT |= (1 << CLR_PIN); 

    // Initialize TWI (I2C)
    twi_init();

    // Initialize MAX5825 DAC
    twi_start();
    twi_write(MAX5825_ADDR); // Address and write command
    twi_write((MAX5825_REG_REF | 0b101)); // DAC channel
    twi_write(0x00);
    twi_write(0x00);
    twi_stop();

    twi_start();
    twi_write(MAX5825_ADDR); // Address and write command
    twi_write(MAX5825_REG_CODEn_LOADall); // DAC channel
    twi_write(0x00);
    twi_write(0x00);
    twi_stop();
}

void twi_init(void) {
    TWSR = 0x00; // Set prescaler to zero
    TWBR = 12; // Baud rate set to 400kHz assuming F_CPU is 16MHz and no prescaler
    TWCR = (1 << TWEN); // Enable TWI
}

void twi_start(void) {
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT))); // Wait for TWINT Flag set. This indicates that the START condition has been transmitted
}

void twi_stop(void) {
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

void twi_write(uint8_t data) {
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT))); // Wait for TWINT Flag set. This indicates that the data has been transmitted, and ACK/NACK has been received
}

void max5825_write(uint8_t channel, uint16_t value) {
    twi_start();
    twi_write(MAX5825_ADDR); // Address and write command
    twi_write(MAX5825_REG_CODEn_LOADn | (channel & 0x0F)); // DAC channel
    // Send the 12-bit data
    // The upper 8 bits of the value (MSB)
    twi_write((uint8_t)((value >> 8) & 0xFF));
    // The lower 4 bits of the value (LSB) shifted into the upper 4 bits of the byte, as the MAX5825 expects it
    twi_write((uint8_t)(value & 0xF0));
    twi_stop();

    // Toggle LDAC to update the DAC output
    CLEAR_BIT(LDAC_PORT, LDAC_PIN); // Set LDAC low to trigger the update
    _delay_us(10); // Wait for the DAC to update
    SET_BIT(LDAC_PORT, LDAC_PIN); // Set LDAC high again
}

int main(void) {
    setup();

    while (1) {
        max5825_write(0, 0xFFFF);
        _delay_ms(500); 
        max5825_write(0, 0x0000);
        _delay_ms(500); 
    }
}
