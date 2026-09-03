#ifndef TOGGLEBUTTON_H
#define TOGGLEBUTTON_H
    #include "input.h"

    /** @brief Max number of ToggleButtons that can exist at once (see MAX_TOGGLEBUTTON_INSTANCES in togglebutton.c). */
    #define MAX_TOGGLEBUTTON_INSTANCES 4

    /**
     * @brief A push button whose logical state toggles once per press,
     * not once per raw edge: a press flips state, pressing again flips
     * it back. Extends Input (Agent <- Input <- ToggleButton, same
     * chain FadeLed uses on Pwm).
     *
     * Debounced with a timestamp, not polling: the interrupt handler
     * (ToggleButton_OnInterrupt, see togglebutton.c) ignores any
     * trigger less than TOGGLEBUTTON_DEBOUNCE_MS after the last
     * accepted one, and toggles state directly, right there in
     * interrupt context - cheap enough (one timestamp compare, two
     * field writes) not to need deferring to a task, so no FreeRTOS
     * task is created for this (timeout == 0 is fine).
     *
     * Up to MAX_TOGGLEBUTTON_INSTANCES buttons can coexist: each one's
     * interrupt is looked up by gpio in a small instance table (see
     * togglebutton.c), the same pattern Peripheral itself uses for
     * gpiosCallback[].
     */
    typedef struct {
        Input input;
        bool state;
        uint32_t lastTriggerMs;
    } ToggleButton;

    /**
     * @brief Allocates a new ToggleButton and wires its pin's interrupt
     * through Peripheral's shared GPIO dispatcher (see
     * OnInGPIOInterrupt() in peripheral.h) rather than grabbing the
     * SDK's single callback slot for itself. Panics if
     * MAX_TOGGLEBUTTON_INSTANCES buttons already exist.
     *
     * @param name Name of the task/agent (copied internally, see Agent_Init).
     * @param pin GPIO pin this button is wired to.
     * @param pullUp true = internal pull-up, false = internal pull-down.
     * @param interruptEvents Which raw edge(s) count as a press attempt
     * (OR of GPIO_IRQ_EDGE_RISE/FALL); each one still goes through the
     * debounce check before actually toggling state.
     * @param uxStackDepth Task stack depth, in words (moot, no task is created).
     * @param uxPriority FreeRTOS task priority (moot, no task is created).
     * @return ToggleButton* Newly heap-allocated, already wired-up button.
     */
    ToggleButton *NewToggleButton(char *name, uint8_t pin, bool pullUp, uint32_t interruptEvents, uint32_t uxStackDepth, UBaseType_t uxPriority);

    /**
     * @brief Current toggle state: true after an odd number of debounced
     * presses, false after an even number (starts false).
     *
     * @param btn Button to read.
     * @return bool Current toggle state.
     */
    bool GetToggleButtonState(ToggleButton *btn);
#endif
