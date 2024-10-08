#define F_CPU 16000000UL
#define BAUD 31250UL
#define MY_UBRR F_CPU / 16 / BAUD - 1

#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>

#define NUM_GATES 8
#define DEBOUNCE_TIME 50
#define LONG_PRESS_TIME 2000
#define TIMER_TICK 10
#define DEBOUNCE_THRESHOLD (DEBOUNCE_TIME / TIMER_TICK)
#define LONG_PRESS_THRESHOLD (LONG_PRESS_TIME / TIMER_TICK)

#define SET_BIT(PORT, PIN) ((PORT) |= (1 << (PIN)))
#define CLEAR_BIT(PORT, PIN) ((PORT) &= ~(1 << (PIN)))

// Gate control
#define GATE_PORT PORTD
#define GATE_PORT_0 PORTB
#define GATE_DDR DDRD
#define GATE_DDR_0 DDRB
#define GATE_PIN_0 PB0
#define GATE_PIN_1 PD1

// LED control
#define LED_PORT PORTC
#define LED_DDR DDRC
#define LED_PIN PC0
#define LED_SET_ON() CLEAR_BIT(LED_PORT, LED_PIN)
#define LED_SET_OFF() SET_BIT(LED_PORT, LED_PIN)

// Button control
#define BUTTON_PORT PORTC
#define BUTTON_DDR DDRC
#define BUTTON_PIN PC1
#define BUTTON_PIN_REG PINC

// TWI (I2C) parameters, these are predefined in the ATmega8 - here for reference
#define SDA_PIN PC4  // SDA pin
#define SCL_PIN PC5  // SCL pin

// LDAC control
#define LDAC_PIN PC2
#define CLR_PIN PC3
#define LDAC_PORT PORTC
#define CLR_PORT PORTC
#define CONTROL_DDR DDRC

// MAX5825 DAC definitions
#define MAX5825_ADDR 0x20  // 0010 000W, where W is the write bit
#define MAX5825_REG_REF 0x20
#define MAX5825_REG_CODEn_LOADall 0xA0
#define MAX5825_REG_CODEn_LOADn 0xB0  // 1011 XXXX, where X encodes the DAC channel

// EEPROM configuration
#define EEPROM_BUTTON_FIX (uint8_t *)0x07

// Button states
#define BUTTON_IDLE 0
#define BUTTON_DEBOUNCING 1
#define BUTTON_PRESSED 2
#define BUTTON_HELD 3
#define BUTTON_DOWN 4
#define BUTTON_RELEASED 5

// LED states
#define LED_OFF 1
#define LED_ON 2
#define LED_BLINK1 3
#define LED_BLINK2 4
#define LED_BLINK3 5
#define LED_BLINK4 6

volatile uint8_t buttonState = BUTTON_IDLE;
volatile uint8_t ledState = LED_OFF;

uint16_t pitch_lookup[61] = {
    0x0000, 0x0440, 0x0880, 0x0CD0, 0x1110, 0x1550, 0x19A0, 0x1DE0, 0x2220, 0x2660, 0x2AA0, 0x2EF0,  // C-2
    0x3330, 0x3770, 0x3BC0, 0x4000, 0x4440, 0x4880, 0x4CC0, 0x5110, 0x5550, 0x5990, 0x5DE0, 0x6220,  // C-1
    0x6660, 0x6AA0, 0x6EE0, 0x7330, 0x7770, 0x7BB0, 0x8000, 0x8440, 0x8880, 0x8CC0, 0x9100, 0x9550,  // C0
    0x9990, 0x9DD0, 0xA220, 0xA660, 0xAAA0, 0xAEE0, 0xB320, 0xB770, 0xBBB0, 0xBFF0, 0xC440, 0xC880,  // C1
    0xCCC0, 0xD100, 0xD550, 0xD990, 0xDDD0, 0xE210, 0xE660, 0xEAA0, 0xEEE0, 0xF320, 0xF760, 0xFBB0,  // C2
    0xFFF0};                                                                                         // C3

typedef struct {
    uint8_t status;
    uint8_t data1;
    uint8_t data2;
    volatile uint8_t ready;
} MIDI_Message;

volatile MIDI_Message midiMsg = {0, 0, 0, 0};

#define MAP_DRUM_VELO 0
#define MAP_DRUM_CC 1
#define MAP_KEYS_PITCH 2

uint8_t midi_map[40] = {
    MAP_DRUM_VELO, 0x80, 0, 0,    0,  MAP_DRUM_VELO,  0x80, 1, 0, 0, MAP_DRUM_VELO, 0x80, 2, 0, 0,
    MAP_DRUM_VELO, 0x80, 3, 0,    0,  MAP_DRUM_VELO,  0x80, 4, 0, 0, MAP_DRUM_VELO, 0x80, 5, 0, 0,
    MAP_DRUM_CC,   0x80, 6, 0xB0, 69, MAP_KEYS_PITCH, 0x81, 0, 0, 0,
};

void setup(void);
void USART_Init(unsigned int ubrr);
void processButton(void);
void updateLED(void);
void setGate(uint8_t gateIndex, uint8_t state);
void twi_init(void);
void twi_start(void);
void twi_stop(void);
void twi_write(uint8_t data);
void max5825_write(uint8_t channel, uint16_t value);
void processMIDI(void);

int main(void) {
    setup();

    while (1) {
        processButton();
        updateLED();
        _delay_ms(TIMER_TICK);
    }
}

void setup() {
    GATE_DDR_0 |= (1 << GATE_PIN_0);  // Set PB0 as output
    GATE_DDR |= 0xFE;                 // Set PD1 to PD7 as outputs (0xFE = 0b11111110)

    DDRC = (1 << LED_PIN) | (1 << BUTTON_PIN);
    LED_SET_OFF();

    // Fix for Hardware Version 1.5, from original code
    while (!eeprom_is_ready());
    if (eeprom_read_byte(EEPROM_BUTTON_FIX) == 0xAA) {
        BUTTON_DDR &= ~(1 << BUTTON_PIN);
    }

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

    USART_Init(MY_UBRR);
    sei();

    for (uint8_t i = 0; i < NUM_GATES; i++) {
        setGate(i, 1);
        _delay_ms(50);
        setGate(i, 0);
    }
}

void USART_Init(unsigned int ubrr) {
    UBRRH = (unsigned char)(ubrr >> 8);
    UBRRL = (unsigned char)ubrr;
    UCSRB = (1 << RXEN) | (1 << RXCIE);
    UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0);
}

ISR(USART_RXC_vect) {
    static uint8_t midiState = 0;
    uint8_t byte = UDR;

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

            processMIDI();
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
    // Wait for TWINT Flag set. This indicates that the START condition has been transmitted
    while (!(TWCR & (1 << TWINT)));
}

void twi_stop(void) { TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN); }

void twi_write(uint8_t data) {
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    // Wait for TWINT Flag set. This indicates that the data has been transmitted, and ACK/NACK has been received
    while (!(TWCR & (1 << TWINT)));
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

void processButton() {
    static uint16_t buttonTimer = 0;
    uint8_t currentReading = BUTTON_PIN_REG & (1 << BUTTON_PIN);

    switch (buttonState) {
        case BUTTON_IDLE:
            if (currentReading) {
                buttonState = BUTTON_DEBOUNCING;
                buttonTimer = 0;
            }
            break;

        case BUTTON_DEBOUNCING:
            if (++buttonTimer >= DEBOUNCE_THRESHOLD) {
                buttonState = BUTTON_PRESSED;
                buttonTimer = 0;
            }
            break;

        case BUTTON_PRESSED:
        case BUTTON_DOWN:
            if (!currentReading) {
                buttonState = (buttonState == BUTTON_PRESSED) ? BUTTON_RELEASED : BUTTON_IDLE;
            } else if (buttonTimer >= LONG_PRESS_THRESHOLD) {
                buttonState = BUTTON_HELD;
            } else {
                buttonTimer++;
            }
            break;

        case BUTTON_HELD:
            if (!currentReading) {
                buttonState = BUTTON_IDLE;
            } else {
                buttonState = BUTTON_DOWN;
                buttonTimer = 0;
            }
            break;

        case BUTTON_RELEASED:
            buttonState = BUTTON_IDLE;
            break;
    }
}

void updateLED(void) {
    static uint16_t ledTimer = 0;
    static uint8_t ledToggle = 0;

    switch (ledState) {
        case LED_ON:
            LED_SET_ON();
            break;
        case LED_OFF:
            LED_SET_OFF();
            break;
        default:
            if (++ledTimer >= (1000 >> (ledState - LED_BLINK1)) / TIMER_TICK) {
                ledToggle = !ledToggle;
                if (ledToggle)
                    LED_SET_ON();
                else
                    LED_SET_OFF();
                ledTimer = 0;
            }
            break;
    }
}

inline void setGate(uint8_t gateIndex, uint8_t state) {
    uint8_t pin = (gateIndex == 0) ? GATE_PIN_0 : GATE_PIN_1 + gateIndex - 1;
    volatile uint8_t *port = (gateIndex == 0) ? &GATE_PORT_0 : &GATE_PORT;
    if (state)
        SET_BIT(*port, pin);
    else
        CLEAR_BIT(*port, pin);
}

void processMIDI() {
    uint8_t gateIndex = 0;
    uint8_t *mapPtr = midi_map;

    while (gateIndex < NUM_GATES) {
        uint8_t mapType = *mapPtr++;
        uint8_t gateCommand = *mapPtr++;
        uint8_t gateValue = *mapPtr++;
        uint8_t cvCommand = *mapPtr++;
        uint8_t cvValue = *mapPtr++;

        // Make NoteOffs into NoteOns and compare
        if ((midiMsg.status & 0xEF) == gateCommand) {
            if (midiMsg.data1 == gateValue || mapType == MAP_KEYS_PITCH) {
                setGate(gateIndex, (midiMsg.status & 0xF0) == 0x90);

                if (mapType == MAP_DRUM_VELO) {
                    max5825_write(gateIndex, (midiMsg.status & 0xF0) == 0x90 ? midiMsg.data2 << 9 : 0);
                } else if (mapType == MAP_KEYS_PITCH) {
                    max5825_write(gateIndex, pitch_lookup[midiMsg.data1 < 61 ? midiMsg.data1 : 60]);
                }
            }
        } else if (midiMsg.status == cvCommand && midiMsg.data1 == cvValue) {
            max5825_write(gateIndex, midiMsg.data2 << 9);
        }

        gateIndex++;
    }

    midiMsg.ready = 0;
}
