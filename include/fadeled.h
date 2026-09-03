#ifndef FADELED_H
#define FADELED_H
    #include "pwm.h"

    /**
     * @brief A breathing/fading LED, as a Pwm extension (which is itself
     * an Agent extension - Agent <- Pwm <- FadeLed, three levels, same
     * chain BlinkLed uses on Output): base (Pwm) plus the fade's own
     * direction (state: false = brightening, true = dimming) and step
     * size. behave() reuses Pwm's own Get/SetPwmValue() to read and
     * write the level, rather than touching the hardware registers
     * itself.
     */
    typedef struct {
        Pwm pwm;
        bool state;
        uint8_t step;
    } FadeLed;

    /**
     * @brief Allocates a new FadeLed and registers it via Pwm_Init()
     * (which itself routes the GPIO to PWM and starts the slice) -
     * its own behave() steps the fade every timeout ms, so it needs a
     * real task (timeout > 0), same as BlinkLed.
     *
     * @param name Name of the task/agent (copied internally, see Agent_Init).
     * @param timeout Period in ms between fade steps.
     * @param uxStackDepth Task stack depth, in words.
     * @param uxPriority FreeRTOS task priority (0 .. configMAX_PRIORITIES-1).
     * @param pin GPIO pin number this LED is wired to.
     * @param step How much the level changes per tick (1-255); panics if 0.
     * @return FadeLed* Newly heap-allocated, already-fading FadeLed.
     */
    FadeLed *NewFadeLed(char *name, uint timeout, uint32_t uxStackDepth, UBaseType_t uxPriority, uint8_t pin, uint8_t step);

#endif
