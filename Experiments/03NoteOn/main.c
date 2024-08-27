#define F_CPU 16000000UL
#define BAUD 31250UL
#define MY_UBRR F_CPU / 16 / BAUD - 1

#include <avr/interrupt.h>
#include <avr/io.h>

// Define the number of gates and their respective hardware configuration
#define NUM_GATES 8
#define GATE_PORT PORTD
#define GATE_PORT_0 PORTB
#define GATE_DDR DDRD
#define GATE_DDR_0 DDRB
#define GATE_PIN_0 PB0
#define GATE_PIN_1 PD1

// MIDI message structure
typedef struct {
    uint8_t status;
    uint8_t data1;
    uint8_t data2;
    volatile uint8_t ready;
} MIDI_Message;

volatile MIDI_Message midiMsg = {0, 0, 0, 0};

// Macros for setting and clearing bits
#define SET_BIT(PORT, PIN) ((PORT) |= (1 << (PIN)))
#define CLEAR_BIT(PORT, PIN) ((PORT) &= ~(1 << (PIN)))

// Function prototypes
void setup(void);
void USART_Init(unsigned int ubrr);
void setGate(uint8_t gateIndex, uint8_t state);
void processMIDI(void);

int main(void) {
    setup();

    while (1) {
    }
}

void setup() {
    GATE_DDR_0 |= (1 << GATE_PIN_0);  // Set PB0 as output
    GATE_DDR |= 0xFE;                 // Set PD1 to PD7 as outputs (0xFE = 0b11111110)

    USART_Init(MY_UBRR);
    sei();
}

void USART_Init(unsigned int ubrr) {
    UBRRH = (unsigned char)(ubrr >> 8);
    UBRRL = (unsigned char)ubrr;
    UCSRB = (1 << RXEN) | (1 << RXCIE);                  // Enable receiver and receive complete interrupt
    UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0);  // 8-bit data
}

ISR(USART_RXC_vect) {
    static uint8_t midiState = 0;
    uint8_t byte = UDR;  // Read the received MIDI byte from the UDR register

    switch (midiState) {
        case 0:
            if (byte >= 0x80 && byte < 0xF0) {  // MIDI command byte
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
            midiMsg.ready = 1;  // Mark message as ready
            midiState = 0;
            processMIDI();
            break;
    }
}

inline void setGate(uint8_t gateIndex, uint8_t state) {
    uint8_t pin = (gateIndex == 0) ? GATE_PIN_0 : GATE_PIN_1 + gateIndex - 1;
    volatile uint8_t* port = (gateIndex == 0) ? &GATE_PORT_0 : &GATE_PORT;
    if (state)
        SET_BIT(*port, pin);
    else
        CLEAR_BIT(*port, pin);
}

void processMIDI() {
    if ((midiMsg.status & 0xF0) == 0x90) {  // Note on
        setGate(midiMsg.data1 % NUM_GATES, 1);
    } else if ((midiMsg.status & 0xF0) == 0x80) {  // Note off
        setGate(midiMsg.data1 % NUM_GATES, 0);
    }
    midiMsg.ready = 0;  // Reset the ready flag
}
