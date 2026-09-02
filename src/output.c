/**
 * @file output.c
 * @brief Implementation of Output: a plain GPIO output Agent with no
 * cached state of its own - Set()/Get() always talk to the hardware
 * directly (see the @brief on the Output struct in output.h for why).
 */

#include "output.h"
#include "hardware/gpio.h"
#include "stdbool.h"
#include "stdlib.h"

/**
 * @brief Output keeps no cache to refresh, so there is nothing periodic
 * to do; this exists only because Agent_Init() requires a non-NULL
 * behave(). Pair it with timeout == 0 so no task is even created for it.
 *
 * @param self The agent itself (unused).
 */
static void Output_Behave(Agent *self) {
    (void)self;
}

/**
 * @brief Drives the pin low before the Output is freed, so deleting one
 * doesn't leave its GPIO in whatever state it last happened to be in.
 *
 * @param self The Output being deleted, as its base Agent.
 */
static void Output_Delete(Agent *self) {
    Output *out = (Output *)self;
    gpio_put(out->gpio, 0);
}

/** @copydoc NewOutput */
Output *NewOutput(char *name, uint timeout, uint32_t uxStackDepth, UBaseType_t uxPriority, uint8_t gpio) {
    Output *out = malloc(sizeof(Output));
    out->gpio = gpio;
    gpio_init(out->gpio);
    gpio_set_dir(out->gpio, true);
    Agent_Init(&out->base, name, timeout, uxStackDepth, uxPriority, Output_Behave, Output_Delete);
    return out;
}

/** @copydoc SetOutputValue */
void SetOutputValue(Output *out, bool value) {
    gpio_put(out->gpio, value);
}

/** @copydoc GetOutputValue */
bool GetOutputValue(Output *out) {
    return gpio_get(out->gpio);
}
