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
#define EEPROM_MIDIMAP (uint8_t *)0x101

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

// Learning/Awaiting CV States
#define NUM_LEARNING_STATES 3
#define LEARNING_VELOCITY 0
#define LEARNING_PITCH 1
#define LEARNING_CC 2
#define AWAITING_CC 3

volatile uint8_t buttonState = BUTTON_IDLE;
volatile uint8_t ledState = LED_OFF;
volatile uint8_t learningMode = 0;
volatile uint8_t learningGateIndex = 0;
volatile uint8_t learningCVState = LEARNING_VELOCITY;

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

#define MIDIMAP_VELOCITY 0
#define MIDIMAP_CC 1
#define MIDIMAP_PITCH 2

uint8_t midi_map[40] = {
    MIDIMAP_VELOCITY, 0x80, 24, 0, 0, MIDIMAP_VELOCITY, 0x80, 25, 0, 0, MIDIMAP_VELOCITY, 0x80, 26, 0, 0,
    MIDIMAP_VELOCITY, 0x80, 27, 0, 0, MIDIMAP_VELOCITY, 0x80, 28, 0, 0, MIDIMAP_VELOCITY, 0x80, 29, 0, 0,
    MIDIMAP_VELOCITY, 0x80, 30, 0, 0, MIDIMAP_VELOCITY, 0x80, 31, 0, 0,
};

void setup(void);
void loadMidiMap(void);
void saveMidiMap(void);
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
void midiLearn(void);

int main(void) {
    setup();

    while (1) {
        processButton();
        updateLED();

        if (buttonState == BUTTON_HELD) {
            learningMode ^= 1;
            ledState = learningMode ? LED_BLINK1 : LED_OFF;
            learningGateIndex = 0;
        }

        if (learningMode) {
            midiLearn();
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

    loadMidiMap();

    for (uint8_t i = 0; i < NUM_GATES; i++) {
        setGate(i, 1);
        _delay_ms(50);
        setGate(i, 0);
    }

    sei();
}

void loadMidiMap() {
    while (!eeprom_is_ready());
    eeprom_read_block(&midi_map, EEPROM_MIDIMAP, 40);
}

void saveMidiMap() {
    while (!eeprom_is_ready());
    eeprom_write_block(&midi_map, EEPROM_MIDIMAP, 40);
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
            midiMsg.ready = 1;
            midiState = 0;
            break;
    }

    if (midiMsg.ready && !learningMode) {
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

inline void processMIDI() {
    uint8_t gateIndex = 0;
    uint8_t *mapPtr = midi_map;
    uint8_t mapType, gateCommand, gateValue, cvCommand1, cvValue1, cvCommand2, cvValue2;
    uint8_t commandFiltered = midiMsg.status & 0xEF; 
    uint8_t noteOnFlag = (midiMsg.status & 0xF0) == 0x90;
    uint8_t data1 = midiMsg.data1;

    while (gateIndex < NUM_GATES) {
        mapType = *mapPtr++;
        gateCommand = *mapPtr++;
        gateValue = *mapPtr++;
        cvCommand1 = *mapPtr++;
        cvValue1 = *mapPtr++;
        cvCommand2 = *mapPtr++;
        cvValue2 = *mapPtr++;

        if (commandFiltered == gateCommand) {
            if (data1 == gateValue || mapType == MIDIMAP_PITCH) {
                setGate(gateIndex, noteOnFlag);

                if (mapType == MIDIMAP_VELOCITY) {
                    max5825_write(gateIndex, noteOnFlag ? midiMsg.data2 << 9 : 0);
                } else if (mapType == MIDIMAP_PITCH) {
                    max5825_write(gateIndex, pitch_lookup[data1 < 61 ? data1 : 60]);
                }
            }
        } else if (midiMsg.status == cvCommand1 && data1 == cvValue1) {
            max5825_write(gateIndex, midiMsg.data2 << 9);
        }

        gateIndex++;
    }

    midiMsg.ready = 0;
}

inline void midiLearn() {
    if (midiMsg.ready) {
        switch (learningCVState) {
            case LEARNING_VELOCITY:
                if ((midiMsg.status & 0xF0) == 0x90) {
                    midi_map[learningGateIndex * 7] = MIDIMAP_VELOCITY;
                    midi_map[learningGateIndex * 7 + 1] = midiMsg.status & 0xEF;
                    midi_map[learningGateIndex * 7 + 2] = midiMsg.data1;

                    setGate(learningGateIndex, 1);
                    learningGateIndex++;
                    learningCVState = LEARNING_VELOCITY;
                    ledState = learningCVState + LED_BLINK1;
                }
                break;
            case LEARNING_PITCH:
                if ((midiMsg.status & 0xF0) == 0x90) {
                    midi_map[learningGateIndex * 7] = MIDIMAP_PITCH;
                    midi_map[learningGateIndex * 7 + 1] = midiMsg.status & 0xEF;

                    setGate(learningGateIndex, 1);
                    learningGateIndex++;
                    learningCVState = LEARNING_VELOCITY;
                    ledState = learningCVState + LED_BLINK1;
                }
                break;
            case LEARNING_CC:
                if ((midiMsg.status & 0xF0) == 0x90) {
                    midi_map[learningGateIndex * 7] = MIDIMAP_CC;
                    midi_map[learningGateIndex * 7 + 1] = midiMsg.status & 0xEF;
                    learningCVState = AWAITING_CC;
                }
                break;
            case AWAITING_CC:
                if ((midiMsg.status & 0xF0) == 0xB0) {
                    midi_map[learningGateIndex * 7 + 3] = midiMsg.status;
                    midi_map[learningGateIndex * 7 + 4] = midiMsg.data1;

                    setGate(learningGateIndex, 1);
                    learningGateIndex++;
                    learningCVState = LEARNING_VELOCITY;
                    ledState = learningCVState + LED_BLINK1;
                }
                break;
            default:
                break;
        }
        midiMsg.ready = 0;
    }

    if (buttonState == BUTTON_RELEASED) {
        switch (learningCVState) {
            case LEARNING_VELOCITY:
            case LEARNING_PITCH:
            case LEARNING_CC:
                learningCVState = (learningCVState + 1) % NUM_LEARNING_STATES;
                break;
            default:
                learningCVState = LEARNING_VELOCITY;
                break;
        }
    }

    switch (learningCVState) {
        case AWAITING_CC:
            ledState = LED_BLINK3;
            break;
        default:
            ledState = learningCVState + LED_BLINK1;
            break;
    }

    if (learningGateIndex == NUM_GATES) {
        for (uint8_t i = 0; i < NUM_GATES; i++) {
            setGate(i, 0);
        }
        learningMode = 0;
        learningCVState = LEARNING_VELOCITY;
        ledState = LED_OFF;
        saveMidiMap();
    }
}