#define F_CPU 16000000UL
#define BAUD 31250UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <util/delay.h>

#define NUM_GATES 8
#define DEBOUNCE_TIME 50
#define LONG_PRESS_TIME 2000
#define TIMER_TICK 10

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

// EEPROM configuration
#define EEPROM_BUTTON_FIX (uint8_t *) 0x07

// Button states
#define BUTTON_IDLE       0
#define BUTTON_DEBOUNCING 1
#define BUTTON_PRESSED    2
#define BUTTON_HELD       3
#define BUTTON_DOWN       4
#define BUTTON_RELEASED   5

// LED states
#define LED_OFF      1
#define LED_ON       2
#define LED_BLINK1   3
#define LED_BLINK2   4
#define LED_BLINK3   5
#define LED_BLINK4   6

volatile uint8_t buttonState = BUTTON_IDLE;
volatile uint8_t ledState = LED_OFF;

void setup(void);
void processButton(void);
void updateLED(void);
void setGate(uint8_t gateIndex, uint8_t state);

int main(void) {
    setup();

    while (1) {
        processButton();

        if (buttonState == BUTTON_HELD){
            ledState = ledState == LED_OFF ? LED_BLINK1 : LED_OFF;
        } else if (buttonState == BUTTON_RELEASED && ledState > LED_ON) {
            ledState = ((ledState - LED_BLINK1 + 1) % 4) + LED_BLINK1;
        }

        updateLED();

        setGate(buttonState, 1);
        _delay_ms(TIMER_TICK); 
        setGate(buttonState, 0);
    }
}

void setup() {
    GATE_DDR_0 |= (1 << GATE_PIN_0); // Set PB0 as output
    GATE_DDR |= 0xFE;                // Set PD1 to PD7 as outputs (0xFE = 0b11111110)

	DDRC = (1 << LED_PIN) | (1 << BUTTON_PIN);
    LED_SET_OFF();

    // Fix for Hardware Version 1.5, from original code
    while (!eeprom_is_ready());
    if (eeprom_read_byte(EEPROM_BUTTON_FIX) == 0xAA) {
        BUTTON_DDR &= ~(1 << BUTTON_PIN);
    }

    for (uint8_t i = 0; i < NUM_GATES; i++) {
        setGate(i, 1); 
        _delay_ms(50); 
        setGate(i, 0); 
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
            if (++buttonTimer >= (DEBOUNCE_TIME / TIMER_TICK)) {
                buttonState = BUTTON_PRESSED;
                buttonTimer = 0;
            }
            break;

        case BUTTON_PRESSED:
        case BUTTON_DOWN:
            if (!currentReading) {
                buttonState = buttonState == BUTTON_PRESSED ? BUTTON_RELEASED : BUTTON_IDLE;
            } else if (buttonTimer >= (LONG_PRESS_TIME / TIMER_TICK)) {
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
                if (ledToggle) LED_SET_ON();
                else LED_SET_OFF();
                ledTimer = 0;
            }
            break;
    }
}

inline void setGate(uint8_t gateIndex, uint8_t state) {
    uint8_t pin = (gateIndex == 0) ? GATE_PIN_0 : GATE_PIN_1 + gateIndex - 1;
    volatile uint8_t* port = (gateIndex == 0) ? &GATE_PORT_0 : &GATE_PORT;
    if (state) SET_BIT(*port, pin);
    else CLEAR_BIT(*port, pin);
}

