#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

#include <avr/io.h>

#define F_CPU 16000000UL
#define BAUD 31250UL
#define MY_UBRR F_CPU / 16 / BAUD - 1

#define TWI_BAUD_RATE 12
#define MAX5825_ADDRESS 0x20

// Gate control
#define NUM_GATES 8
#define GATE_PORT_D PORTD
#define GATE_PORT_B PORTB
#define GATE_DDR_D DDRD
#define GATE_DDR_B DDRB
#define GATE_PIN_0 PB0
#define GATE_PIN_1 PD1

// LED control
#define LED_PORT PORTC
#define LED_DDR DDRC
#define LED_PIN PC0

// Button control
#define BUTTON_PORT PORTC
#define BUTTON_DDR DDRC
#define BUTTON_PIN PC1
#define BUTTON_PIN_REG PINC

// I2C pins
#define SDA_PIN PC4
#define SCL_PIN PC5

// DAC control pins
#define LDAC_PIN PC2
#define CLR_PIN PC3
#define LDAC_PORT PORTC
#define CLR_PORT PORTC
#define CONTROL_DDR DDRC

// EEPROM configuration
#define EEPROM_BUTTON_FIX (uint8_t *)0x07
#define EEPROM_MIDIMAP (uint8_t *)0x101

#endif