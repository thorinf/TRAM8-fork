#include "io.h"

void updateButton(Button *button) {
    uint8_t currentReading = button->getReading();

    switch (button->buttonState) {
        case BUTTON_IDLE:
            if (currentReading) {
                button->buttonState = BUTTON_DEBOUNCING;
                button->buttonTimer = 0;
            }
            break;

        case BUTTON_DEBOUNCING:
            if (++(button->buttonTimer) >= DEBOUNCE_THRESHOLD) {
                button->buttonState = BUTTON_PRESSED;
                button->buttonTimer = 0;
            }
            break;

        case BUTTON_PRESSED:
        case BUTTON_DOWN:
            if (!currentReading) {
                button->buttonState = (button->buttonState == BUTTON_PRESSED) ? BUTTON_RELEASED : BUTTON_IDLE;
            } else if (button->buttonTimer >= LONG_PRESS_THRESHOLD) {
                button->buttonState = BUTTON_HELD;
            } else {
                button->buttonTimer++;
            }
            break;

        case BUTTON_HELD:
            if (!currentReading) {
                button->buttonState = BUTTON_IDLE;
            } else {
                button->buttonState = BUTTON_DOWN;
                button->buttonTimer = 0;
            }
            break;

        case BUTTON_RELEASED:
            button->buttonState = BUTTON_IDLE;
            break;
    }
}

void updateLED(LED *led) {
    uint16_t blinkRate = (200 >> (led->ledState - LED_BLINK1)) / TIMER_TICK;

    switch (led->ledState) {
        case LED_ON:
            led->ledOn();
            break;
        case LED_OFF:
            led->ledOff();
            break;
        default:
            led->ledTimer++;

            if (led->pauseState) {
                led->ledOff();
                if (led->ledTimer >= blinkRate * led->ledBlinkCount) {
                    led->ledTimer = 0;
                    led->pauseState = 0;
                }
            } else {
                if (led->ledTimer >= blinkRate) {
                    led->ledTimer = 0;
                    led->ledToggle = !led->ledToggle;

                    if (led->ledToggle) {
                        led->ledOn();
                    } else {
                        led->ledOff();
                        if (++led->current_blink >= led->ledBlinkCount) {
                            led->pauseState = 1;
                            led->current_blink = 0;
                        }
                    }
                }
            }
            break;
    }
}
