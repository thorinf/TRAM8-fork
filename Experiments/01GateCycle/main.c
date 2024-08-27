// Define the CPU frequency and MIDI baud rate
#define F_CPU 16000000UL
#define BAUD 31250UL

#include <avr/io.h>
#include <util/delay.h>

// Define the number of gates and their respective hardware configuration
#define NUM_GATES 8
#define GATE_PORT PORTD
#define GATE_PORT_0 PORTB
#define GATE_DDR DDRD
#define GATE_DDR_0 DDRB
#define GATE_PIN_0 PB0
#define GATE_PIN_1 PD1

// Macros for setting and clearing bits
#define SET_BIT(PORT, PIN) ((PORT) |= (1 << (PIN)))
#define CLEAR_BIT(PORT, PIN) ((PORT) &= ~(1 << (PIN)))

// Function prototypes
void setup(void);
void setGate(uint8_t gateIndex, uint8_t state);

void setup() {
    // Set gate pins as outputs
    GATE_DDR_0 |= (1 << GATE_PIN_0);  // Set PB0 as output
    GATE_DDR |= 0xFE;                 // Set PD1 to PD7 as outputs (0xFE = 0b11111110)
}

void setGate(uint8_t gateIndex, uint8_t state) {
    uint8_t pin = (gateIndex == 0) ? GATE_PIN_0 : GATE_PIN_1 + gateIndex - 1;
    volatile uint8_t* port = (gateIndex == 0) ? &GATE_PORT_0 : &GATE_PORT;
    if (state) {
        SET_BIT(*port, pin);
    } else {
        CLEAR_BIT(*port, pin);
    }
}

int main(void) {
    setup();

    while (1) {
        static uint8_t ctrlIndex = 0;
        setGate(ctrlIndex, 1);
        _delay_ms(100);
        setGate(ctrlIndex, 0);
        ctrlIndex = (ctrlIndex + 1) % NUM_GATES;
    }
}
