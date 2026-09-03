/**
 * @file input.c
 * @brief Implementation of Input: a plain GPIO input Agent with no
 * cached state of its own - GetInputValue() always talks to the
 * hardware directly (see the @brief on the Input struct in input.h
 * for why). Optionally wires a caller-supplied interrupt callback
 * straight into the SDK (see the @warning on NewInput in input.h).
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
 * @brief Releases the pin before the Input is freed: disables any
 * interrupt trigger still armed on it, detaches it from the GPIO/SIO
 * peripheral (gpio_deinit) and clears its pull resistor, so deleting
 * an Input doesn't leave it configured - or listening - behind.
 *
 * @warning gpio_deinit() alone does NOT disable a pin's interrupt:
 * that's a separate register (see gpio_set_irq_enabled()) untouched by
 * a function-select change. Without this, the pin is left floating
 * (function NULL, pulls disabled) with its interrupt still armed -
 * electrical noise on a floating pin can trigger a genuine spurious
 * interrupt for an Input that no longer exists.
 *
 * @param self The Input being deleted, as its base Agent.
 */
static void Input_Delete(Agent *self) {
    Input *in = (Input *)self;
    gpio_set_irq_enabled(in->gpio, GPIO_IRQ_LEVEL_LOW | GPIO_IRQ_LEVEL_HIGH | GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, false);
    gpio_disable_pulls(in->gpio);
    gpio_deinit(in->gpio);
}

/** @copydoc Input_Init */
void Input_Init(Input *in, uint8_t pin, bool pullUp, uint32_t interruptEvents, void (*callback)(uint gpio, uint32_t events), AgentBehaviour behave, AgentDelete delete, char *name, uint timeout, uint32_t stack, UBaseType_t prio) {
    in->gpio = pin;
    gpio_init(in->gpio);
    gpio_set_dir(in->gpio, false);
    if (pullUp) {
        gpio_pull_up(in->gpio);
    } else {
        gpio_pull_down(in->gpio);
    }
    if (callback != NULL) {
        gpio_set_irq_enabled_with_callback(in->gpio, interruptEvents, true, callback);
    }
    Agent_Init(&in->base, name, timeout, stack, prio, behave, delete);
}

/** @copydoc NewInput */
Input *NewInput(char *name, uint timeout, uint32_t uxStackDepth, UBaseType_t uxPriority, uint8_t gpio, bool pullUp, uint32_t interruptEvents, void (*callback)(uint gpio, uint32_t events)) {
    Input *in = malloc(sizeof(Input));
    Input_Init(in, gpio, pullUp, interruptEvents, callback, Input_Behave, Input_Delete, name, timeout, uxStackDepth, uxPriority);
    return in;
}

/** @copydoc GetInputValue */
bool GetInputValue(Input *in) {
    return gpio_get(in->gpio);
}
