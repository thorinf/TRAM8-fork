#include <avr/interrupt.h>
#include <avr/io.h>

// Define the CPU frequency and MIDI baud rate
#define F_CPU 16000000UL
#define BAUD 31250UL
#define MY_UBRR F_CPU / 16 / BAUD - 1

// TWI (I2C) parameters, these are predefined in the ATmega8 - here for reference
#define SDA_PIN PC4  // SDA pin
#define SCL_PIN PC5  // SCL pin

// Pins for additional control
#define LDAC_PIN PC2
#define CLR_PIN PC3
#define LDAC_PORT PORTC
#define CLR_PORT PORTC
#define CONTROL_DDR DDRC

// MAX5825 DAC Definitions
#define MAX5825_ADDR 0x20  // 0010 000W, where W is the write bit
#define MAX5825_REG_REF 0x20
#define MAX5825_REG_CODEn_LOADall 0xA0
#define MAX5825_REG_CODEn_LOADn 0xB0  // 1011 XXXX, where X encodes the DAC channel

typedef struct {
    uint8_t status;
    uint8_t data1;
    uint8_t data2;
    volatile uint8_t ready;
} MIDI_Message;

volatile MIDI_Message midiMsg = {0, 0, 0, 0};  // Ensure the structure is volatile

// Lookup table for pitch values
uint16_t pitch_lookup[61] = {
    0x0000, 0x0440, 0x0880, 0x0CD0, 0x1110, 0x1550, 0x19A0, 0x1DE0, 0x2220, 0x2660, 0x2AA0, 0x2EF0,  // C-2
    0x3330, 0x3770, 0x3BC0, 0x4000, 0x4440, 0x4880, 0x4CC0, 0x5110, 0x5550, 0x5990, 0x5DE0, 0x6220,  // C-1
    0x6660, 0x6AA0, 0x6EE0, 0x7330, 0x7770, 0x7BB0, 0x8000, 0x8440, 0x8880, 0x8CC0, 0x9100, 0x9550,  // C0
    0x9990, 0x9DD0, 0xA220, 0xA660, 0xAAA0, 0xAEE0, 0xB320, 0xB770, 0xBBB0, 0xBFF0, 0xC440, 0xC880,  // C1
    0xCCC0, 0xD100, 0xD550, 0xD990, 0xDDD0, 0xE210, 0xE660, 0xEAA0, 0xEEE0, 0xF320, 0xF760, 0xFBB0,  // C2
    0xFFF0,                                                                                          // C3
};

// Macros for setting and clearing bits
#define SET_BIT(PORT, PIN) ((PORT) |= (1 << (PIN)))
#define CLEAR_BIT(PORT, PIN) ((PORT) &= ~(1 << (PIN)))

// Function prototypes
void setup(void);
void USART_Init(unsigned int ubrr);
ISR(USART_RXC_vect);
void twi_init(void);
void twi_start(void);
void twi_stop(void);
void twi_write(uint8_t data);
void max5825_write(uint8_t channel, uint16_t value);

void setup() {
    // Initialize control pins for DAC
    CONTROL_DDR |= (1 << LDAC_PIN) | (1 << CLR_PIN);  // Set LDAC and CLR as outputs
    // Set LDAC high to hold the DAC output (active-low signal, high means no update)
    LDAC_PORT |= (1 << LDAC_PIN);
    // Set CLR high to avoid clearing the DAC (active-low signal, high means no clear)
    CLR_PORT |= (1 << CLR_PIN);

    // Initialize TWI (I2C)
    twi_init();

    // Initialize MAX5825 DAC
    twi_start();
    twi_write(MAX5825_ADDR);               // Address and write command
    twi_write((MAX5825_REG_REF | 0b101));  // DAC channel
    twi_write(0x00);
    twi_write(0x00);
    twi_stop();

    twi_start();
    twi_write(MAX5825_ADDR);               // Address and write command
    twi_write(MAX5825_REG_CODEn_LOADall);  // DAC channel
    twi_write(0x00);
    twi_write(0x00);
    twi_stop();
}

void USART_Init(unsigned int ubrr) {
    UBRRH = (unsigned char)(ubrr >> 8);
    UBRRL = (unsigned char)ubrr;
    UCSRB = (1 << RXEN) | (1 << RXCIE);
    UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0);
}

ISR(USART_RXC_vect) {
    static uint8_t midiState = 0;
    uint8_t byte = UDR;  // Read the received MIDI byte from the UDR register

    // Parse the MIDI messages that are 3 bytes long
    switch (midiState) {
        case 0:
            if (byte >= 0x80 && byte < 0xF0) {
                midiMsg.status = byte;
                midiState = 1;
            }
            break;
        case 1:
            midiMsg.data1 = byte;
            midiState = 2;
            break;
        case 2:
            midiMsg.data2 = byte;
            midiMsg.ready = 1;  // Message is ready
            midiState = 0;
            break;
    }
}

void twi_init(void) {
    TWSR = 0x00;         // Set prescaler to zero
    TWBR = 12;           // Baud rate set to 400kHz assuming F_CPU is 16MHz and no prescaler
    TWCR = (1 << TWEN);  // Enable TWI
}

void twi_start(void) {
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR &
             (1 << TWINT)));  // Wait for TWINT Flag set. This indicates that the START condition has been transmitted
}

void twi_stop(void) { TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN); }

void twi_write(uint8_t data) {
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));  // Wait for TWINT Flag set. This indicates that the data has been transmitted, and
                                     // ACK/NACK has been received
}

void max5825_write(uint8_t channel, uint16_t value) {
    twi_start();
    twi_write(MAX5825_ADDR);                                // Address and write command
    twi_write(MAX5825_REG_CODEn_LOADn | (channel & 0x0F));  // DAC channel
    // Send the 12-bit data
    // The upper 8 bits of the value (MSB)
    twi_write((uint8_t)((value >> 8) & 0xFF));
    // The lower 4 bits of the value (LSB) shifted into the upper 4 bits of the byte, as the MAX5825 expects it
    twi_write((uint8_t)(value & 0xF0));
    twi_stop();
}

int main(void) {
    setup();
    USART_Init(MY_UBRR);
    sei();

    while (1) {
        if (midiMsg.ready) {
            // Process MIDI message here
            uint8_t channel = midiMsg.status & 0x0F;
            uint8_t command = midiMsg.status & 0xF0;
            if (command == 0x90 && midiMsg.data2 > 0) {  // Note on message with non-zero velocity
                if (midiMsg.data1 < 61) {                // Ensure the note is within the range of the lookup table
                    max5825_write(channel, pitch_lookup[midiMsg.data1]);
                }
            }
            midiMsg.ready = 0;
        }
    }
}
