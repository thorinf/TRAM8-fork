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
#include "io.h"

#define IS_NOTE_ON(command) (((command) & 0xF0) == 0x90)

#define AWAITING_CC NUM_MIDIMAP_TYPES
#define AWAITING_PITCH NUM_MIDIMAP_TYPES + 1
#define AWAITING_STEP NUM_MIDIMAP_TYPES + 2
#define AWAITING_RESET NUM_MIDIMAP_TYPES + 3

typedef struct {
    uint8_t status;
    uint8_t data1;
    uint8_t data2;
    volatile uint8_t ready;
} MIDI_Message;

Button learnButton = {BUTTON_IDLE, 0, read_button};
LED learnLED = {LED_OFF, 1, 0, 0, 0, 0, led_on, led_off};
volatile MIDI_Message midiMsg = {0, 0, 0, 0};
MIDIMapEntry midi_map[NUM_GATES];
uint16_t dac_buffer[NUM_GATES];
uint16_t lfsr_seeds[NUM_GATES];
volatile uint8_t subRoutine = 0;

void setup(void);
void USART_Init(unsigned int ubrr);
void saveMidiMap(MIDIMapEntry *src, uint8_t *location);
void loadMidiMap(MIDIMapEntry *dst, uint8_t *location);
void copyMidiMap(MIDIMapEntry *src, MIDIMapEntry *dst);
void sysExMidiMap(MIDIMapEntry *dst);
void newSeeds(void);
void resetDacBuffer(void);
void handleMIDIMessage(void);
void midiLearn(void);

void setup() {
    pin_initialize();
    twi_init();
    max5825_init();
    USART_Init(MY_UBRR);
    loadMidiMap(midi_map, EEPROM_MIDIMAP);

    for (uint8_t i = 0; i < NUM_GATES; i++) {
        lfsr_seeds[i] = (i + 1) << 4;
        gate_set(i, 1);
        _delay_ms(50);
    }
    gate_set_multiple(0xFF, 0);

    sei();
}

int main(void) {
    setup();
    uint8_t menuState;

    while (1) {
        updateButton(&learnButton);
        updateLED(&learnLED);
        _delay_ms(TIMER_TICK);

        switch (subRoutine) {
            case 0:  // Normal play
                if (learnButton.buttonState == BUTTON_RELEASED)
                    newSeeds();
                else if (learnButton.buttonState == BUTTON_HELD) {
                    menuState = 0;
                    gate_set(menuState, 1);
                    subRoutine = 1;
                }
                break;
            case 1:  // In Menu
                if (learnButton.buttonState == BUTTON_RELEASED) {
                    gate_set(menuState, 0);
                    menuState = (menuState + 1) % 6;
                    gate_set(menuState, 1);
                } else if (learnButton.buttonState == BUTTON_HELD) {
                    gate_set(menuState, 0);

                    switch (menuState) {
                        case 0:
                            learnLED.ledState = LED_BLINK1;
                            learnLED.ledBlinkCount = MIDIMAP_VELOCITY + 1;
                            subRoutine = 2;
                            break;
                        case 1:
                            saveMidiMap(midi_map, EEPROM_MIDIMAP);
                            subRoutine = 0;
                            break;
                        case 2:
                            loadMidiMap(midi_map, EEPROM_MIDIMAP);
                            subRoutine = 0;
                            break;
                        case 3:
                            copyMidiMap(midi_map, midi_map_bsp);
                            subRoutine = 0;
                            break;
                        case 4:
                            sysExMidiMap(midi_map);
                            subRoutine = 0;
                            break;
                        case 5:
                            subRoutine = 0;
                            break;
                    }
                }
                break;
            case 2:  // Learning
                midiLearn();
                break;
        }
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
            if (!subRoutine) {
                handleMIDIMessage();
            }
            break;
    }
}

void saveMidiMap(MIDIMapEntry *src, uint8_t *location) {
    while (!eeprom_is_ready());
    eeprom_write_block((const void *)src, (void *)location, sizeof(midi_map));
}

void loadMidiMap(MIDIMapEntry *dst, uint8_t *location) {
    while (!eeprom_is_ready());
    eeprom_read_block((void *)dst, (const void *)location, sizeof(midi_map));
}

void copyMidiMap(MIDIMapEntry *src, MIDIMapEntry *dst) {
    for (uint8_t i = 0; i < NUM_GATES; i++) {
        dst[i] = src[i];
    }
}

void sysExMidiMap(MIDIMapEntry *dst) {
    uint8_t sysExBuffer[114];
    uint8_t *bufferPtr = sysExBuffer;

    cli();
    gate_set_multiple(0x81, 1);

    for (uint8_t i = 0; i < sizeof(sysExBuffer); i++) {
        while (!(UCSRA & (1 << RXC)));
        sysExBuffer[i] = UDR;
    }

    gate_set_multiple(0x81, 0);

    if ((sysExBuffer[0] != 0xF0) || (sysExBuffer[113] != 0xF7)) {
        gate_set_multiple(0xFF, 1);
        _delay_ms(50);
        gate_set_multiple(0xFF, 0);
        sei();
        return;
    }

    uint8_t index = 1;
    for (uint8_t i = 0; i < NUM_GATES; i++) {
        dst[i].mapType = sysExBuffer[index] | (sysExBuffer[index + 1] << 7);
        dst[i].gateCommand = sysExBuffer[index + 2] | (sysExBuffer[index + 3] << 7);
        dst[i].gateValue = sysExBuffer[index + 4] | (sysExBuffer[index + 5] << 7);
        dst[i].cvCommand1 = sysExBuffer[index + 6] | (sysExBuffer[index + 7] << 7);
        dst[i].cvValue1 = sysExBuffer[index + 8] | (sysExBuffer[index + 9] << 7);
        dst[i].cvCommand2 = sysExBuffer[index + 10] | (sysExBuffer[index + 11] << 7);
        dst[i].cvValue2 = sysExBuffer[index + 12] | (sysExBuffer[index + 13] << 7);

        index += 14;

        gate_set_multiple(i, 1);
        _delay_ms(50);
    }
    gate_set_multiple(0xFF, 0);
    sei();
}

void resetDacBuffer() {
    for (uint8_t i = 0; i < NUM_GATES; i++) {
        uint8_t mapType = midi_map[i].mapType;
        if (mapType == MIDIMAP_RANDSEQ || mapType == MIDIMAP_RANDSEQ_SAH) {
            dac_buffer[i] = lfsr_seeds[i];
        }
    }
}

void newSeeds() {
    for (uint8_t i = 0; i < NUM_GATES; i++) {
        uint8_t mapType = midi_map[i].mapType;
        if (mapType == MIDIMAP_RANDSEQ || mapType == MIDIMAP_RANDSEQ_SAH) {
            updateLfsrAlt(&lfsr_seeds[i]);
        }
    }
}

inline void handleMIDIMessage() {
    uint8_t gateIndex = 0;
    uint8_t commandFiltered = midiMsg.status & 0xEF;
    uint8_t noteOnFlag = IS_NOTE_ON(midiMsg.status);
    uint8_t data1 = midiMsg.data1;

    while (gateIndex < NUM_GATES) {
        MIDIMapEntry *mapEntry = &midi_map[gateIndex];
        uint8_t gateCommand = mapEntry->gateCommand & 0xEF;

        switch (mapEntry->mapType) {
            case MIDIMAP_VELOCITY:
                if (commandFiltered == gateCommand && data1 == mapEntry->gateValue) {
                    gate_set(gateIndex, noteOnFlag);
                    max5825_write(gateIndex, noteOnFlag ? midiMsg.data2 << 9 : 0);  // 7-bit to 16-bit
                }
                break;

            case MIDIMAP_CC:
                if (commandFiltered == gateCommand && data1 == mapEntry->gateValue) {
                    gate_set(gateIndex, noteOnFlag);
                } else if (midiMsg.status == mapEntry->cvCommand1 && data1 == mapEntry->cvValue1) {
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
                if (commandFiltered == gateCommand && data1 == mapEntry->gateValue) {
                    gate_set(gateIndex, noteOnFlag);
                    max5825_write(gateIndex, dac_buffer[gateIndex]);
                } else if (commandFiltered == mapEntry->cvCommand1 && data1 < PITCH_SIZE) {
                    dac_buffer[gateIndex] = pitch_lookup[data1];
                }
                break;

            case MIDIMAP_RANDSEQ:
                if (commandFiltered == gateCommand && data1 == mapEntry->gateValue) {
                    gate_set(gateIndex, noteOnFlag);
                }
                if (midiMsg.status == mapEntry->cvCommand1 && data1 == mapEntry->cvValue1) {
                    max5825_write(gateIndex, updateLfsr(&dac_buffer[gateIndex]));
                } else if (midiMsg.status == mapEntry->cvCommand2 && data1 == mapEntry->cvValue2) {
                    dac_buffer[gateIndex] = lfsr_seeds[gateIndex];
                }
                break;

            case MIDIMAP_RANDSEQ_SAH:
                if (commandFiltered == gateCommand && data1 == mapEntry->gateValue) {
                    gate_set(gateIndex, noteOnFlag);
                    max5825_write(gateIndex, dac_buffer[gateIndex]);
                }
                if (midiMsg.status == mapEntry->cvCommand1 && data1 == mapEntry->cvValue1) {
                    updateLfsr(&dac_buffer[gateIndex]);
                } else if (midiMsg.status == mapEntry->cvCommand2 && data1 == mapEntry->cvValue2) {
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
    uint8_t nextGateFlag = 0;

    if (midiMsg.ready) {
        MIDIMapEntry *mapEntry = &midi_map[learningIndex];
        switch (learningMapType) {
            case MIDIMAP_VELOCITY:
                if (IS_NOTE_ON(midiMsg.status)) {
                    mapEntry->mapType = learningMapType;
                    mapEntry->gateCommand = midiMsg.status;
                    mapEntry->gateValue = midiMsg.data1;
                    nextGateFlag = 1;
                }
                break;
            case MIDIMAP_CC:
                if (IS_NOTE_ON(midiMsg.status)) {
                    mapEntry->mapType = learningMapType;
                    mapEntry->gateCommand = midiMsg.status;
                    mapEntry->gateValue = midiMsg.data1;
                    learningMapType = AWAITING_CC;
                    learnLED.ledState = LED_BLINK2;
                }
                break;
            case MIDIMAP_PITCH:
                if (IS_NOTE_ON(midiMsg.status)) {
                    mapEntry->mapType = learningMapType;
                    mapEntry->gateCommand = midiMsg.status;
                    nextGateFlag = 1;
                }
                break;
            case MIDIMAP_PITCH_SAH:
                if (IS_NOTE_ON(midiMsg.status)) {
                    mapEntry->mapType = learningMapType;
                    mapEntry->gateCommand = midiMsg.status;
                    mapEntry->gateValue = midiMsg.data1;
                    learningMapType = AWAITING_PITCH;
                    learnLED.ledState = LED_BLINK2;
                }
                break;
            case MIDIMAP_RANDSEQ:
            case MIDIMAP_RANDSEQ_SAH:
                if (IS_NOTE_ON(midiMsg.status)) {
                    mapEntry->mapType = learningMapType;
                    mapEntry->gateCommand = midiMsg.status;
                    learningMapType = AWAITING_STEP;
                    learnLED.ledState = LED_BLINK2;
                }
                break;
            case AWAITING_CC:
                if ((midiMsg.status & 0xF0) == 0xB0) {
                    mapEntry->cvCommand1 = midiMsg.status;
                    mapEntry->cvValue1 = midiMsg.data1;
                    nextGateFlag = 1;
                }
                break;
            case AWAITING_PITCH:
                if (IS_NOTE_ON(midiMsg.status)) {
                    mapEntry->cvCommand1 = midiMsg.status;
                    nextGateFlag = 1;
                }
                break;
            case AWAITING_STEP:
                if (IS_NOTE_ON(midiMsg.status)) {
                    mapEntry->cvCommand1 = midiMsg.status;
                    mapEntry->cvValue1 = midiMsg.data1;
                    learningMapType = AWAITING_RESET;
                    learnLED.ledState = LED_BLINK3;
                }
                break;
            case AWAITING_RESET:
                if (IS_NOTE_ON(midiMsg.status)) {
                    mapEntry->cvCommand2 = midiMsg.status;
                    mapEntry->cvValue2 = midiMsg.data1;
                    nextGateFlag = 1;
                }
                break;
            default:
                break;
        }
        midiMsg.ready = 0;
    }

    if (learnButton.buttonState == BUTTON_RELEASED) {
        switch (learningMapType) {
            case MIDIMAP_VELOCITY:
            case MIDIMAP_CC:
            case MIDIMAP_PITCH:
            case MIDIMAP_PITCH_SAH:
            case MIDIMAP_RANDSEQ:
                learningMapType++;
                learnLED.ledState = LED_BLINK1;
                learnLED.ledBlinkCount = learningMapType + 1;
                break;
            default:
                learningMapType = MIDIMAP_VELOCITY;
                learnLED.ledState = LED_BLINK1;
                learnLED.ledBlinkCount = learningMapType + 1;
                break;
        }
    }

    if (nextGateFlag) {
        gate_set(learningIndex, 1);
        learningIndex++;
        learningMapType = MIDIMAP_VELOCITY;
        learnLED.ledState = LED_BLINK1;
        learnLED.ledBlinkCount = 1;
    }

    if (learningIndex == NUM_GATES || learnButton.buttonState == BUTTON_HELD) {
        gate_set_multiple(0xFF, 1);
        _delay_ms(50);
        gate_set_multiple(0xFF, 0);

        learningMapType = MIDIMAP_VELOCITY;
        learnLED.ledState = LED_OFF;
        learnLED.ledBlinkCount = 1;
        learningIndex = 0;
        subRoutine = 0;
    }
}