/**
 * @file adc.c
 * @brief Implementation of Adc: an analog input Agent with no cached
 * value of its own - GetAdcValue() always talks to the hardware
 * directly (see the @brief on the Adc struct in adc.h for why).
 */

#include "adc.h"
#include <stdlib.h>
#include "hardware/gpio.h"
#include "hardware/adc.h"

/**
 * @brief Adc keeps no cache to refresh, so there is nothing periodic
 * to do; this exists only because Agent_Init() requires a non-NULL
 * behave(). Pair it with timeout == 0 so no task is even created for it.
 *
 * @param self The agent itself (unused).
 */
static void Adc_Behave(Agent *self) {
    (void)self;
}

/**
 * @brief Releases the GPIO pin before the Adc is freed. There is no
 * adc_deinit() in the SDK (the ADC peripheral itself is shared, not
 * owned per-channel), so the only thing this Adc owns exclusively is
 * its pin - gpio_deinit() detaches it from the analog input (undoing
 * adc_gpio_init()) the same way every other Delete in this project
 * releases its own pin.
 *
 * @param self The Adc being deleted, as its base Agent.
 */
static void Adc_Delete(Agent *self) {
    Adc *adc = (Adc *)self;
    gpio_deinit(adc->gpio);
}

/** @copydoc Adc_Init */
void Adc_Init(Adc *adc, uint8_t pin, uint8_t channel, AgentBehaviour behave, AgentDelete delete, char *name, uint timeout, uint32_t stack, UBaseType_t prio) {
    adc_init();
    adc_gpio_init(pin);
    adc->gpio = pin;
    adc->channel = channel;
    Agent_Init(&adc->base, name, timeout, stack, prio, behave, delete);
}

/** @copydoc NewAdc */
Adc *NewAdc(char *name, uint timeout, uint32_t uxStackDepth, UBaseType_t uxPriority, uint8_t pin, uint8_t channel) {
    Adc *adc = malloc(sizeof(Adc));
    Adc_Init(adc, pin, channel, Adc_Behave, Adc_Delete, name, timeout, uxStackDepth, uxPriority);
    return adc;
}

/** @copydoc GetAdcValue */
uint16_t GetAdcValue(Adc *adc) {
    adc_select_input(adc->channel);
    return adc_read();
}
