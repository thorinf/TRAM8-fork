// Define the CPU frequency and MIDI baud rate
#define F_CPU 16000000UL
#define BAUD 31250UL
#define MY_UBRR F_CPU/16/BAUD-1

#include <avr/interrupt.h>
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

#define MIDI_MSG_SIZE 3  // Maximum size of a MIDI message

typedef struct {
    uint8_t status;
    uint8_t data1;
    uint8_t data2;
    uint8_t ready;
} MIDI_Message;

MIDI_Message midiMsg = {0, 0, 0, 0};

// Function prototypes
void setup(void);
void setGate(uint8_t gateIndex, uint8_t state);

void setup() {
    // Set gate pins as outputs
    GATE_DDR_0 |= (1 << GATE_PIN_0);
    for (uint8_t i = GATE_PIN_1; i <= GATE_PIN_1 + NUM_GATES - 2; i++) {
        GATE_DDR |= (1 << i);
    }
}

void USART_Init(unsigned int ubrr) {
    UBRRH = (unsigned char)(ubrr >> 8);
    UBRRL = (unsigned char)ubrr;
    UCSRB = (1 << RXEN) | (1 << RXCIE);
    UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0);
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

ISR(USART_RXC_vect) {
    static uint8_t byte_count = 0;
    unsigned char byte = UDR;  // Read the received MIDI byte from the UDR register

    // If the byte is a system message (starting with 0xF), ignore it
    if (byte_count == 0 & byte >= 0xF0) {
        return;
    }

    if (byte >= 0x80) {  // Status byte start
        byte_count = 0;
        midiMsg.status = byte;
    } else if (byte_count == 1) {
        midiMsg.data1 = byte;
    } else if (byte_count == 2) {
        midiMsg.data2 = byte;
        midiMsg.ready = 1;  // Message is ready
    }
    byte_count++;
}

int main(void) {
    setup();
    USART_Init(MY_UBRR);
    sei();

    while (1) {
        if (midiMsg.ready) {
            // Process MIDI message here
            // Example: Set gate based on note on/off
            if ((midiMsg.status & 0xF0) == 0x90) {  // Note on message
                setGate(midiMsg.data1 % NUM_GATES, 1);  // Set gate based on note
            } else if ((midiMsg.status & 0xF0) == 0x80) {  // Note off message
                setGate(midiMsg.data1 % NUM_GATES, 0);  // Clear gate based on note
            }

            midiMsg.ready = 0;  // Reset the ready flag
        }
    }
}
