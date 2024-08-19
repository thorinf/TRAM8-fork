#define F_CPU 16000000UL
#define BAUD 31250UL

#include <avr/io.h>
#include <avr/eeprom.h>
#include <util/delay.h>

#define NUM_GATES 8
#define GATE_PORT PORTD
#define GATE_PORT_0 PORTB
#define GATE_DDR DDRD
#define GATE_DDR_0 DDRB
#define GATE_PIN_0 PB0
#define GATE_PIN_1 PD1

#define LED_PORT PORTC
#define LED_DDR DDRC
#define LED_PIN PC0

#define BUTTON_PORT PORTC
#define BUTTON_DDR DDRC
#define BUTTON_PIN PC1
#define BUTTON_PIN_REG PINC

#define SET_BIT(PORT, PIN) ((PORT) |= (1 << (PIN)))
#define CLEAR_BIT(PORT, PIN) ((PORT) &= ~(1 << (PIN)))

#define LED_ON() CLEAR_BIT(LED_PORT, LED_PIN)
#define LED_OFF() SET_BIT(LED_PORT, LED_PIN)

#define EEPROM_BUTTON_FIX (uint8_t *) 0x07
#define EEPROM_GATE_ADDR (uint8_t *) 0x100 

// Function prototypes
void setup(void);
void setGate(uint8_t gateIndex, uint8_t state);

void setup() {
    // Set gate pins as outputs
    GATE_DDR_0 |= (1 << GATE_PIN_0); // Set PB0 as output
    GATE_DDR |= 0xFE;                // Set PD1 to PD7 as outputs (0xFE = 0b11111110)

	DDRC = (1 << LED_PIN) | (1 << BUTTON_PIN);

    // Fix for Hardware Version 1.5, from original code
    while (!eeprom_is_ready());
	uint8_t buttonfix_flag = eeprom_read_byte(EEPROM_BUTTON_FIX);

	if (buttonfix_flag == 0xAA)	{
        // Set the button pin as input
		DDRC &= ~((1 << BUTTON_PIN));
	}

    for (uint8_t i = 0; i < NUM_GATES; i++) {
        setGate(i, 1); 
        _delay_ms(50); 
        setGate(i, 0); 
    }
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

    uint8_t ctrlIndex = eeprom_read_byte(EEPROM_GATE_ADDR) % 8;
    setGate(ctrlIndex, 1);

    while (1) {
        uint8_t button_current_state = BUTTON_PIN_REG & (1 << BUTTON_PIN);

        if (button_current_state) {
            setGate(ctrlIndex, 0);
            ctrlIndex = (ctrlIndex + 1) % 8; 

            while (!eeprom_is_ready());

            eeprom_write_byte(EEPROM_GATE_ADDR, ctrlIndex);

            setGate(ctrlIndex, 1);
        }

        _delay_ms(50);  
    }
}
