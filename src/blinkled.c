/**
 * @file blinkled.c
 * @brief Implementation of BlinkLed: a periodically-toggling GPIO
 * output, driven by its own FreeRTOS task via behave().
 */

#include <stdlib.h>
#include "hardware/gpio.h"
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
    SetOutputValue(&led->base, led->state);
}

/**
 * @brief Turns the LED off, then releases its pin, before the BlinkLed
 * is freed: SetOutputValue(false) leaves it dark for the brief moment
 * it's still SIO-driven, and gpio_deinit() then detaches the pin from
 * the SIO peripheral entirely, so deleting a BlinkLed doesn't leave its
 * pin claimed as an output behind.
 *
 * @param self The BlinkLed being deleted, as its base Agent.
 */
static void Blinked_Delete(Agent *self) {
    BlinkLed *led = (BlinkLed *)self;
    led->state = false;
    SetOutputValue(&led->base, false);
    gpio_deinit(led->base.gpio);
}


BlinkLed *NewBlinkLed(char *name, int timeout, uint32_t stack, UBaseType_t prio, uint8_t pin) {
    BlinkLed *led = malloc(sizeof(BlinkLed));
    led->state = false;
    Output_Init(&led->base, pin, BlinkLed_Behave, Blinked_Delete, name, timeout, stack, prio);
    return led;
}
