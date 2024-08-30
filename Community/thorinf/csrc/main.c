#define F_CPU 16000000UL
#define BAUD 31250UL
#define MY_UBRR F_CPU / 16 / BAUD - 1

#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>

#include "hardware_config.h"
#include "max5825_control.h"
#include "midimap.h"
#include "pin_control.h"
#include "pitch.h"
#include "random.h"
#include "twi_control.h"

#define DEBOUNCE_TIME 50
#define LONG_PRESS_TIME 2000
#define TIMER_TICK 10
#define DEBOUNCE_THRESHOLD (DEBOUNCE_TIME / TIMER_TICK)
#define LONG_PRESS_THRESHOLD (LONG_PRESS_TIME / TIMER_TICK)

#define IS_NOTE_ON(command) (((command) & 0xF0) == 0x90)

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

// Awaiting Learning
#define AWAITING_CC NUM_MIDIMAP_TYPES
#define AWAITING_PITCH NUM_MIDIMAP_TYPES + 1
#define AWAITING_STEP NUM_MIDIMAP_TYPES + 2
#define AWAITING_RESET NUM_MIDIMAP_TYPES + 3

volatile uint8_t buttonState = BUTTON_IDLE;
volatile uint8_t ledState = LED_OFF;
volatile uint8_t ledBlinkCount = 1;
volatile uint8_t inMenu = 0;

typedef struct {
    uint8_t status;
    uint8_t data1;
    uint8_t data2;
    volatile uint8_t ready;
} MIDI_Message;

volatile MIDI_Message midiMsg = {0, 0, 0, 0};

uint8_t midi_map[56];
uint8_t *midi_map_preset = midi_map_bsp;
uint16_t dac_buffer[NUM_GATES];
uint16_t lfsr_seeds[NUM_GATES];

void setup(void);
void USART_Init(unsigned int ubrr);
void processButton(void);
void updateLED(void);
void menu(void);
void loadMidiMap(void);
void saveMidiMap(void);
void copyMidiMap(void);
void newSeeds(void);
void resetDacBuffer(void);
void processMIDI(void);
void midiLearn(void);

void setup() {
    pin_initialize();
    twi_init();
    max5825_init();
    USART_Init(MY_UBRR);
    loadMidiMap();

    for (uint8_t i = 0; i < NUM_GATES; i++) {
        lfsr_seeds[i] = (i + 1) << 4;

        gate_set(i, 1);
        _delay_ms(50);
        gate_set(i, 0);
    }

    sei();
}

int main(void) {
    setup();

    while (1) {
        processButton();
        updateLED();

        if (buttonState == BUTTON_RELEASED) {
            newSeeds();
        } else if (buttonState == BUTTON_HELD) {
            menu();
        }

        _delay_ms(TIMER_TICK);
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
                midiState = (byte & 0xF0) == 0xD0 ? 1 : 2;
            }
            break;
        case 1:
            midiMsg.data1 = byte;
            midiState = 0;
            break;
        case 2:
            midiMsg.data1 = byte;
            midiState = 3;
            break;
        case 3:
            midiMsg.data2 = byte;
            midiMsg.ready = 1;
            midiState = 0;
            if (!inMenu) {
                processMIDI();
            }
            break;
    }
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
    static uint8_t currentBlink = 0;
    static uint8_t pauseState = 0;
    uint16_t blinkRate = (200 >> (ledState - LED_BLINK1)) / TIMER_TICK;

    switch (ledState) {
        case LED_ON:
            led_on();
            break;
        case LED_OFF:
            led_off();
            break;
        default:
            ledTimer++;

            if (pauseState) {
                led_off();
                if (ledTimer >= blinkRate * ledBlinkCount) {
                    ledTimer = 0;
                    pauseState = 0;
                }
            } else {
                if (ledTimer >= blinkRate) {
                    ledTimer = 0;
                    ledToggle = !ledToggle;

                    if (ledToggle) {
                        led_on();
                    } else {
                        led_off();
                        if (++currentBlink >= ledBlinkCount) {
                            pauseState = 1;
                            currentBlink = 0;
                        }
                    }
                }
            }
            break;
    }
}

void menu() {
    uint8_t menuState = 0;
    uint8_t isLearning = 0;

    inMenu = 1;
    midiMsg.ready = 0;
    gate_set(menuState, 1);

    while (inMenu) {
        processButton();
        updateLED();

        if (isLearning) {
            ledBlinkCount = 1;
            ledState = LED_BLINK1;
            midiLearn();
        } else if (buttonState == BUTTON_RELEASED) {
            gate_set(menuState, 0);
            menuState = (menuState + 1) % 5;
            gate_set(menuState, 1);
        } else if (buttonState == BUTTON_HELD) {
            switch (menuState) {
                case 0:
                    gate_set(menuState, 0);
                    isLearning = 1;
                    break;
                case 1:
                    saveMidiMap();
                    gate_set(menuState, 0);
                    inMenu = 0;
                    break;
                case 2:
                    loadMidiMap();
                    gate_set(menuState, 0);
                    inMenu = 0;
                    break;
                case 3:
                    copyMidiMap();
                    gate_set(menuState, 0);
                    inMenu = 0;
                    break;
                case 4:
                    gate_set(menuState, 0);
                    inMenu = 0;
                    break;
            }
        }
        _delay_ms(TIMER_TICK);
    }
}

void saveMidiMap() {
    while (!eeprom_is_ready());
    eeprom_write_block(&midi_map, EEPROM_MIDIMAP, 56);
}

void loadMidiMap() {
    while (!eeprom_is_ready());
    eeprom_read_block(&midi_map, EEPROM_MIDIMAP, 56);
}

void copyMidiMap() {
    for (uint8_t i = 0; i < sizeof(midi_map); i++) {
        midi_map[i] = midi_map_preset[i];
    }
}

void resetDacBuffer() {
    uint8_t *mapPtr = midi_map;
    for (uint8_t i = 0; i < NUM_GATES; i++, mapPtr += 7) {
        uint8_t mapType = *mapPtr;
        if (mapType == MIDIMAP_RANDSEQ || mapType == MIDIMAP_RANDSEQ_SAH) {
            dac_buffer[i] = lfsr_seeds[i];
        }
    }
}

void newSeeds() {
    uint8_t *mapPtr = midi_map;
    for (uint8_t i = 0; i < NUM_GATES; i++, mapPtr += 7) {
        uint8_t mapType = *mapPtr;
        if (mapType == MIDIMAP_RANDSEQ || mapType == MIDIMAP_RANDSEQ_SAH) {
            updateLfsrAlt(&lfsr_seeds[i]);
        }
    }
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
        gateCommand = *mapPtr++ & 0xEF;
        gateValue = *mapPtr++;
        cvCommand1 = *mapPtr++;
        cvValue1 = *mapPtr++;
        cvCommand2 = *mapPtr++;
        cvValue2 = *mapPtr++;

        switch (mapType) {
            case MIDIMAP_VELOCITY:
                if (commandFiltered == gateCommand && data1 == gateValue) {
                    gate_set(gateIndex, noteOnFlag);
                    max5825_write(gateIndex, noteOnFlag ? midiMsg.data2 << 9 : 0);
                }
                break;
            case MIDIMAP_CC:
                if (commandFiltered == gateCommand && data1 == gateValue) {
                    gate_set(gateIndex, noteOnFlag);
                } else if (midiMsg.status == cvCommand1 && data1 == cvValue1) {
                    max5825_write(gateIndex, midiMsg.data2 << 9);
                }
                break;
            case MIDIMAP_PITCH:
                if (commandFiltered == gateCommand && data1 < PITCH_SIZE) {
                    gate_set(gateIndex, noteOnFlag);
                    max5825_write(gateIndex, pitch_lookup[data1]);
                }
                break;
            case MIDIMAP_PITCH_SAH:
                if (commandFiltered == gateCommand && data1 == gateValue) {
                    gate_set(gateIndex, noteOnFlag);
                    max5825_write(gateIndex, dac_buffer[gateIndex]);
                } else if (commandFiltered == cvCommand1 && data1 < PITCH_SIZE) {
                    dac_buffer[gateIndex] = pitch_lookup[data1];
                }
                break;
            case MIDIMAP_RANDSEQ:
                if (commandFiltered == gateCommand && data1 == gateValue) {
                    gate_set(gateIndex, noteOnFlag);
                }
                if (midiMsg.status == cvCommand1 && data1 == cvValue1) {
                    max5825_write(gateIndex, updateLfsr(&dac_buffer[gateIndex]));
                } else if (midiMsg.status == cvCommand2 && data1 == cvValue2) {
                    dac_buffer[gateIndex] = lfsr_seeds[gateIndex];
                }
                break;
            case MIDIMAP_RANDSEQ_SAH:
                if (commandFiltered == gateCommand && data1 == gateValue) {
                    gate_set(gateIndex, noteOnFlag);
                    max5825_write(gateIndex, dac_buffer[gateIndex]);
                }
                if (midiMsg.status == cvCommand1 && data1 == cvValue1) {
                    updateLfsr(&dac_buffer[gateIndex]);
                } else if (midiMsg.status == cvCommand2 && data1 == cvValue2) {
                    dac_buffer[gateIndex] = lfsr_seeds[gateIndex];
                }
                break;
        }

        gateIndex++;
    }

    midiMsg.ready = 0;
}

inline void midiLearn() {
    static uint8_t learningIndex = 0;
    static uint8_t learningMapType = MIDIMAP_VELOCITY;

    if (midiMsg.ready) {
        switch (learningMapType) {
            case MIDIMAP_VELOCITY:
                if (IS_NOTE_ON(midiMsg.status)) {
                    midi_map[learningIndex * 7] = learningMapType;
                    midi_map[learningIndex * 7 + 1] = midiMsg.status;
                    midi_map[learningIndex * 7 + 2] = midiMsg.data1;

                    gate_set(learningIndex, 1);
                    learningIndex++;
                    learningMapType = MIDIMAP_VELOCITY;
                    ledState = LED_BLINK1;
                    ledBlinkCount = 1;
                }
                break;
            case MIDIMAP_CC:
                if (IS_NOTE_ON(midiMsg.status)) {
                    midi_map[learningIndex * 7] = learningMapType;
                    midi_map[learningIndex * 7 + 1] = midiMsg.status;
                    midi_map[learningIndex * 7 + 2] = midiMsg.data1;
                    learningMapType = AWAITING_CC;
                    ledState = LED_BLINK2;
                }
                break;
            case MIDIMAP_PITCH:
                if (IS_NOTE_ON(midiMsg.status)) {
                    midi_map[learningIndex * 7] = learningMapType;
                    midi_map[learningIndex * 7 + 1] = midiMsg.status;

                    gate_set(learningIndex, 1);
                    learningIndex++;
                    learningMapType = MIDIMAP_VELOCITY;
                    ledState = LED_BLINK1;
                    ledBlinkCount = 1;
                }
                break;
            case MIDIMAP_PITCH_SAH:
                if (IS_NOTE_ON(midiMsg.status)) {
                    midi_map[learningIndex * 7] = learningMapType;
                    midi_map[learningIndex * 7 + 1] = midiMsg.status;
                    midi_map[learningIndex * 7 + 2] = midiMsg.data1;
                    learningMapType = AWAITING_PITCH;
                    ledState = LED_BLINK2;
                }
            case MIDIMAP_RANDSEQ:
            case MIDIMAP_RANDSEQ_SAH:
                if (IS_NOTE_ON(midiMsg.status)) {
                    midi_map[learningIndex * 7] = learningMapType;
                    midi_map[learningIndex * 7 + 1] = midiMsg.status;
                    learningMapType = AWAITING_STEP;
                    ledState = LED_BLINK2;
                }
                break;
            case AWAITING_CC:
                if ((midiMsg.status & 0xF0) == 0xB0) {
                    midi_map[learningIndex * 7 + 3] = midiMsg.status;
                    midi_map[learningIndex * 7 + 4] = midiMsg.data1;

                    gate_set(learningIndex, 1);
                    learningIndex++;
                    learningMapType = MIDIMAP_VELOCITY;
                    ledState = LED_BLINK1;
                    ledBlinkCount = 1;
                }
                break;
            case AWAITING_PITCH:
                if (IS_NOTE_ON(midiMsg.status)) {
                    midi_map[learningIndex * 7 + 3] = midiMsg.status;

                    gate_set(learningIndex, 1);
                    learningIndex++;
                    learningMapType = MIDIMAP_VELOCITY;
                    ledState = LED_BLINK1;
                    ledBlinkCount = 1;
                }
                break;
            case AWAITING_STEP:
                if (IS_NOTE_ON(midiMsg.status)) {
                    midi_map[learningIndex * 7 + 3] = midiMsg.status;
                    midi_map[learningIndex * 7 + 4] = midiMsg.data1;
                    learningMapType = AWAITING_RESET;
                    ledState = LED_BLINK3;
                }
                break;
            case AWAITING_RESET:
                if (IS_NOTE_ON(midiMsg.status)) {
                    midi_map[learningIndex * 7 + 5] = midiMsg.status;
                    midi_map[learningIndex * 7 + 6] = midiMsg.data1;

                    gate_set(learningIndex, 1);
                    learningIndex++;
                    learningMapType = MIDIMAP_VELOCITY;
                    ledState = LED_BLINK1;
                    ledBlinkCount = 1;
                }
                break;
            default:
                break;
        }
        midiMsg.ready = 0;
    }

    if (buttonState == BUTTON_RELEASED) {
        switch (learningMapType) {
            case MIDIMAP_VELOCITY:
            case MIDIMAP_CC:
            case MIDIMAP_PITCH:
            case MIDIMAP_PITCH_SAH:
            case MIDIMAP_RANDSEQ:
                learningMapType++;
                ledState = LED_BLINK1;
                ledBlinkCount = learningMapType;
                break;
            default:
                learningMapType = MIDIMAP_VELOCITY;
                ledState = LED_BLINK1;
                ledBlinkCount = learningMapType;
                break;
        }
    }

    if (learningIndex == NUM_GATES || buttonState == BUTTON_HELD) {
        for (uint8_t i = 0; i < NUM_GATES; i++) {
            gate_set(i, 1);
        }
        _delay_ms(50);
        for (uint8_t i = 0; i < NUM_GATES; i++) {
            gate_set(i, 0);
        }
        learningMapType = MIDIMAP_VELOCITY;
        ledState = LED_OFF;
        ledBlinkCount = 1;
        inMenu = 0;
        learningIndex = 0;
    }
}