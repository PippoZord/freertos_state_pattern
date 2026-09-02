/**
 * @file pwm.c
 * @brief Implementation of Pwm: a PWM output Agent with no cached
 * level of its own - Get/SetPwmValue always talk to the hardware
 * directly (see the @brief on the Pwm struct in pwm.h for why).
 */

#include "pwm.h"
#include <stdlib.h>
#include "hardware/gpio.h"
#include "hardware/pwm.h"

/**
 * @brief Pwm keeps no cache to refresh, so there is nothing periodic
 * to do; this exists only because Agent_Init() requires a non-NULL
 * behave(). Pair it with timeout == 0 so no task is even created for it.
 *
 * @param self The agent itself (unused).
 */
static void Pwm_Behave(Agent *self) {
    (void)self;
}

/**
 * @brief Stops the PWM slice and releases the pin before the Pwm is
 * freed. gpio_put() alone would NOT be enough here: the pin's function
 * is GPIO_FUNC_PWM, not GPIO_FUNC_SIO, so gpio_put() would only write
 * the (unused, while PWM-routed) SIO output latch and leave the PWM
 * signal running untouched. Disabling the slice stops the signal;
 * gpio_deinit() then detaches the pin from the PWM peripheral too.
 *
 * @param self The Pwm being deleted, as its base Agent.
 */
static void Pwm_Delete(Agent *self) {
    Pwm *pwm = (Pwm *)self;
    pwm_set_enabled(pwm_gpio_to_slice_num(pwm->gpio), false);
    gpio_deinit(pwm->gpio);
}

/** @copydoc NewPwm */
Pwm *NewPwm(char *name, uint timeout, uint32_t uxStackDepth, UBaseType_t uxPriority, uint8_t pin) {
    Pwm *pwm = malloc(sizeof(Pwm));
    pwm->gpio = pin;
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pin);
    pwm_set_wrap(slice_num, 255);
    pwm_set_enabled(slice_num, true);
    Agent_Init(&pwm->base, name, timeout, uxStackDepth, uxPriority, Pwm_Behave, Pwm_Delete);
    return pwm;
}

/** @copydoc GetPwmValue */
uint16_t GetPwmValue(Pwm *pwm) {
    uint slice_num = pwm_gpio_to_slice_num(pwm->gpio);
    uint channel = pwm_gpio_to_channel(pwm->gpio);

    // Il registro CC dello slice ha due meta': canale A nei bit bassi,
    // canale B in quelli alti - stesso registro per entrambi i canali.
    pwm_slice_hw_t *slice_hw = &pwm_hw->slice[slice_num];
    if (channel == PWM_CHAN_A) {
        return (uint16_t)(slice_hw->cc & PWM_CH0_CC_A_BITS);
    } else {
        return (uint16_t)((slice_hw->cc & PWM_CH0_CC_B_BITS) >> PWM_CH0_CC_B_LSB);
    }
}

/** @copydoc SetPwmValue */
void SetPwmValue(Pwm *pwm, uint16_t level) {
    pwm_set_gpio_level(pwm->gpio, level);
}
