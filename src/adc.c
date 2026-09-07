/**
 * @file adc.c
 * @brief Implementation of Adc: an analog input Agent with no cached
 * value of its own - GetAdcValue() always talks to the hardware
 * directly (see the @brief on the Adc struct in adc.h for why).
 */

#include "adc.h"
#include <stdlib.h>
#include <stdbool.h>
#include "hardware/gpio.h"
#include "hardware/adc.h"

/** @copydoc Adc_Behave */
void Adc_Behave(Agent *self) {
    (void)self;
}

/** @copydoc Adc_Delete */
void Adc_Delete(Agent *self) {
    Adc *adc = (Adc *)self;
    if (adc->gpio != ADC_GPIO_NONE) {
        gpio_deinit(adc->gpio);
    }
}

/**
 * @brief Whether adc_init() has already run. It resets the whole
 * shared ADC peripheral (see hardware_adc's adc_init(): a full
 * reset_unreset plus adc_hw->cs = ADC_CS_EN_BITS, which clears every
 * other bit - including the temperature sensor's enable bit set by a
 * previously-constructed InternalTemperature). Since every Adc/
 * InternalTemperature shares one physical ADC, Adc_Init() must only
 * call adc_init() the first time, or constructing a second Adc-family
 * object would silently reset state a sibling object already set up.
 */
static bool adc_initialized = false;

/** @copydoc Adc_Init */
void Adc_Init(Adc *adc, uint8_t pin, uint8_t channel, AgentBehaviour behave, AgentDelete delete, char *name, uint timeout, uint32_t stack, UBaseType_t prio) {
    if (!adc_initialized) {
        adc_init();
        adc_initialized = true;
    }
    if (pin != ADC_GPIO_NONE) {
        adc_gpio_init(pin);
    }
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
