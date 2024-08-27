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

typedef struct {
    uint8_t status;
    uint8_t data1;
    uint8_t data2;
    volatile uint8_t ready;
} MIDI_Message;

volatile MIDI_Message midiMsg = {0, 0, 0, 0};

uint8_t midi_map[48] = {
    0x80, 24, 0x90, 32, 0x90, 33,  // Gate C0, Step G#0, Reset A0
    0x80, 25, 0x90, 34, 0x90, 35,  // Gate C#0, Step A#0, Reset B0
    0x80, 26, 0x90, 36, 0x90, 37,  // Gate D0, Step C1, Reset C#1
    0x80, 27, 0x90, 38, 0x90, 39,  // Gate D#0, Step D1, Reset D#1
    0x80, 28, 0x90, 40, 0x90, 41,  // Gate E0, Step E1, Reset F1
    0x80, 29, 0x90, 42, 0x90, 43,  // Gate F0, Step F#1, Reset G1
    0x80, 30, 0x90, 44, 0x90, 45,  // Gate F#0, Step G#1, Reset A1
    0x80, 31, 0x90, 46, 0x90, 47,  // Gate G0, Step A#1, Reset B1
};

uint16_t lfsr_seeds[NUM_GATES];
uint16_t lfsr_array[NUM_GATES];

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
uint16_t updateRand(uint16_t *lfsr);
uint16_t updateRandAlt(uint16_t *lfsr);

int main(void) {
    setup();

    while (1) {
        processButton();
        updateLED();

        if (buttonState == BUTTON_RELEASED) {
            for (uint8_t i = 0; i < NUM_GATES; i++) {
                lfsr_seeds[i] = updateRandAlt(&lfsr_array[i]);
            }
        }

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

    for (uint8_t i = 0; i < NUM_GATES; i++) {
        lfsr_seeds[i] = (i + 1) << 4;
        lfsr_array[i] = (i + 1) << 4;

        setGate(i, 1);
        _delay_ms(50);
        setGate(i, 0);
    }

    sei();
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

            break;
    }

    if (midiMsg.ready) {
        processMIDI();
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

inline uint16_t updateRand(uint16_t *lfsr) {
    uint16_t bit = ((*lfsr >> 0) ^ (*lfsr >> 2) ^ (*lfsr >> 3) ^ (*lfsr >> 5)) & 1;
    *lfsr = (*lfsr >> 1) | (bit << 15);
    return *lfsr;
}

inline uint16_t updateRandAlt(uint16_t *lfsr) {
    uint16_t bit = ((*lfsr >> 1) ^ (*lfsr >> 3) ^ (*lfsr >> 4) ^ (*lfsr >> 8)) & 1;
    *lfsr = (*lfsr >> 1) | (bit << 15);
    return *lfsr;
}

inline void processMIDI() {
    uint8_t gateIndex = 0;
    uint8_t *mapPtr = midi_map;
    uint8_t gateCommand, gateValue, cvCommand1, cvValue1, cvCommand2, cvValue2;
    uint8_t commandFiltered = midiMsg.status & 0xEF;
    uint8_t noteOnFlag = (midiMsg.status & 0xF0) == 0x90;
    uint8_t data1 = midiMsg.data1;

    while (gateIndex < NUM_GATES) {
        gateCommand = *mapPtr++;
        gateValue = *mapPtr++;
        cvCommand1 = *mapPtr++;
        cvValue1 = *mapPtr++;
        cvCommand2 = *mapPtr++;
        cvValue2 = *mapPtr++;

        if (commandFiltered == gateCommand && data1 == gateValue) {
            setGate(gateIndex, noteOnFlag);
        }
        if (midiMsg.status == cvCommand1 && data1 == cvValue1) {
            uint16_t rand = updateRand(&lfsr_array[gateIndex]);
            max5825_write(gateIndex, rand);
        }
        if (midiMsg.status == cvCommand2 && data1 == cvValue2) {
            lfsr_array[gateIndex] = lfsr_seeds[gateIndex];
        }

        gateIndex++;
    }

    midiMsg.ready = 0;
}
