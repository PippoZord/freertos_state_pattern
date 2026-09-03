/**
 * @file fadeled.c
 * @brief Implementation of FadeLed: a PWM output that autonomously
 * breathes between 0 and 255, extending Pwm the same way BlinkLed
 * extends Output (Agent <- Pwm <- FadeLed).
 */

#include "fadeled.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include <stdlib.h>
#include <stdio.h>

/**
 * @brief Steps the fade by one tick: brightens (state == false) up to
 * 255, where state flips to true and it dims back down to 0, where
 * state flips back to false - a breathing effect. The next level is
 * computed once, already clamped to [0, 255], and written with a
 * single SetPwmValue() call (unlike an earlier version of this
 * function, which computed a clamped value and then immediately threw
 * it away by unconditionally overwriting it with the raw, unclamped
 * pwm_v +/- step - which could over/underflow the uint16_t level
 * SetPwmValue() takes).
 *
 * @param self The FadeLed being run, as its base Agent.
 */
static void FadeLed_Behave(Agent *self) {
    FadeLed *fled = (FadeLed *)self;
    uint16_t pwm_v = GetPwmValue(&fled->pwm);

    if (pwm_v >= 255) {
        fled->state = true;
    } else if (pwm_v == 0) {
        fled->state = false;
    }

    uint16_t next;
    if (!fled->state) {
        next = (pwm_v + fled->step > 255) ? 255 : (uint16_t)(pwm_v + fled->step);
    } else {
        next = (pwm_v < fled->step) ? 0 : (uint16_t)(pwm_v - fled->step);
    }
    SetPwmValue(&fled->pwm, next);
}

/**
 * @brief Stops the PWM slice and releases the pin before the FadeLed
 * is freed - same cleanup Pwm_Delete does (see pwm.c), since FadeLed
 * doesn't add any hardware of its own beyond the Pwm it extends.
 *
 * @param self The FadeLed being deleted, as its base Agent.
 */
static void FadeLed_Delete(Agent *self) {
    FadeLed *fled = (FadeLed *)self;
    pwm_set_enabled(pwm_gpio_to_slice_num(fled->pwm.gpio), false);
    gpio_deinit(fled->pwm.gpio);
}

/** @copydoc NewFadeLed */
FadeLed *NewFadeLed(char *name, uint timeout, uint32_t uxStackDepth, UBaseType_t uxPriority, uint8_t pin, uint8_t step) {
    if (step < 1) panic("step in FadeLed cannot be less than 1");
    FadeLed *fled = malloc(sizeof(FadeLed));
    // Set before Pwm_Init(): timeout > 0 means a real task starts
    // inside Pwm_Init() (via Agent_Init), and FadeLed_Behave() reads
    // these fields - see the @warning on Agent_Init() in agent.h.
    fled->state = false;
    fled->step = step;
    Pwm_Init(&fled->pwm, pin, FadeLed_Behave, FadeLed_Delete, name, timeout, uxStackDepth, uxPriority);
    SetPwmValue(&fled->pwm, 0);
    return fled;
}
