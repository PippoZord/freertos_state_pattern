/**
 * @file input.c
 * @brief Implementation of Input: a plain GPIO input Agent with no
 * cached state of its own - GetInputValue() always talks to the
 * hardware directly (see the @brief on the Input struct in input.h
 * for why).
 */

#include "input.h"
#include "hardware/gpio.h"
#include "stdbool.h"
#include "stdlib.h"

/**
 * @brief Input keeps no cache to refresh, so there is nothing periodic
 * to do; this exists only because Agent_Init() requires a non-NULL
 * behave(). Pair it with timeout == 0 so no task is even created for it.
 *
 * @param self The agent itself (unused).
 */
static void Input_Behave(Agent *self) {
    (void)self;
}

/**
 * @brief Releases the pin before the Input is freed: detaches it from
 * the GPIO/SIO peripheral (gpio_deinit) and clears its pull resistor,
 * so deleting an Input doesn't leave it configured behind.
 *
 * @param self The Input being deleted, as its base Agent.
 */
static void Input_Delete(Agent *self) {
    Input *in = (Input *)self;
    gpio_disable_pulls(in->gpio);
    gpio_deinit(in->gpio);
}

/** @copydoc NewInput */
Input *NewInput(char *name, uint timeout, uint32_t uxStackDepth, UBaseType_t uxPriority, uint8_t gpio, bool pullUp) {
    Input *in = malloc(sizeof(Input));
    in->gpio = gpio;
    gpio_init(in->gpio);
    gpio_set_dir(in->gpio, false);
    pullUp ? gpio_pull_up(in->gpio) : gpio_pull_down(in->gpio);
    Agent_Init(&in->base, name, timeout, uxStackDepth, uxPriority, Input_Behave, Input_Delete);
    return in;
}

/** @copydoc GetInputValue */
bool GetInputValue(Input *in) {
    return gpio_get(in->gpio);
}
