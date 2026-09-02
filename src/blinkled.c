/**
 * @file blinkled.c
 * @brief Implementation of BlinkLed: a periodically-toggling GPIO
 * output, driven by its own FreeRTOS task via behave().
 */

#include <stdlib.h>
#include "blinkled.h"

/**
 * @brief Toggles the LED's pin and flips the cached state to match -
 * called every timeout ms by Run() (see agent.c).
 *
 * @param self The BlinkLed being run, as its base Agent.
 */
static void BlinkLed_Behave(Agent *self) {
    BlinkLed *led = (BlinkLed *)self;
    led->state = !led->state;
    gpio_put(led->pin, led->state);
}

/**
 * @brief Turns the LED off before the BlinkLed is freed, so deleting
 * one doesn't leave it lit.
 *
 * @param self The BlinkLed being deleted, as its base Agent.
 */
static void Blinked_Delete(Agent *self) {
    BlinkLed *led = (BlinkLed *)self;
    led->state = 0;
    gpio_put(led->pin, 0);
}

/** @copydoc NewBlinkLed */
BlinkLed *NewBlinkLed(char *name, int timeout, uint32_t uxStackDepth, UBaseType_t uxPriority, uint8_t pin) {
    BlinkLed *led = malloc(sizeof(BlinkLed));
    led->pin = pin;
    led->state = false;
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    Agent_Init(&led->base, name, timeout, uxStackDepth, uxPriority, BlinkLed_Behave, Blinked_Delete);
    return led;
}
