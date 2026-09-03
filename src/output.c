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
 * @brief Drives the pin low, then releases it, before the Output is
 * freed: gpio_put(0) leaves it in a defined state for the brief moment
 * it's still SIO-driven, and gpio_deinit() then detaches it from the
 * SIO peripheral entirely, so deleting an Output doesn't leave its pin
 * claimed as an output behind - matching Input_Delete/Pwm_Delete.
 *
 * @param self The Output being deleted, as its base Agent.
 */
static void Output_Delete(Agent *self) {
    Output *out = (Output *)self;
    gpio_put(out->gpio, 0);
    gpio_deinit(out->gpio);
}


void Output_Init(Output *out, uint8_t pin, AgentBehaviour behave, AgentDelete delete, char *name, uint timeout, uint32_t stack, UBaseType_t prio) {
    out->gpio = pin;
    gpio_init(pin);
    gpio_set_dir(pin, true);
    Agent_Init(&out->base, name, timeout, stack, prio, behave, delete);
}

Output *NewOutput(char *name, uint timeout, uint32_t stack, UBaseType_t prio, uint8_t pin) {
    Output *out = malloc(sizeof(Output));
    Output_Init(out, pin, Output_Behave, Output_Delete, name, timeout, stack, prio);
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
