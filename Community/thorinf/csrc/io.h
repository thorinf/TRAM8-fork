#ifndef CONTROL_H
#define CONTROL_H

#include <avr/io.h>

#define DEBOUNCE_TIME 50
#define LONG_PRESS_TIME 2000
#define TIMER_TICK 10
#define DEBOUNCE_THRESHOLD (DEBOUNCE_TIME / TIMER_TICK)
#define LONG_PRESS_THRESHOLD (LONG_PRESS_TIME / TIMER_TICK)

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

typedef struct {
    uint8_t buttonState;
    uint16_t buttonTimer;
    uint8_t (*getReading)(void);
} Button;

typedef struct {
    uint8_t ledState;
    uint8_t ledBlinkCount;
    uint8_t ledToggle;
    uint16_t ledTimer;
    uint8_t current_blink;
    uint8_t pauseState;
    void (*ledOn)(void);
    void (*ledOff)(void);
} LED;

void updateButton(Button *button);
void updateLED(LED *led);

#endif