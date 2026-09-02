#ifndef INPUT_H
#define INPUT_H
    #include "agent.h"
    #include "stdbool.h"

    /**
     * @brief A digital GPIO input, as an Agent extension: base (Agent)
     * plus the GPIO pin number it reads. Deliberately has no cached
     * value field - the pin is only ever read through GetInputValue(),
     * so it always goes straight to the hardware and is already exact;
     * a cache here would just be one more thing that could drift out of
     * sync with reality for no benefit.
     */
    typedef struct {
        Agent base;
        uint8_t gpio;
    }Input;

    /**
     * @brief Allocates a new Input, configures its GPIO as an input with
     * the requested pull resistor, and registers it via Agent_Init().
     * Pass timeout == 0: Input_Behave() has nothing to do (no cache to
     * refresh), so no FreeRTOS task is needed for it.
     *
     * @param name Name of the task/agent (copied internally, see Agent_Init).
     * @param timeout Period in ms between behave() calls; use 0 (no task).
     * @param uxStackDepth Task stack depth, in words (moot if timeout == 0).
     * @param uxPriority FreeRTOS task priority (moot if timeout == 0).
     * @param pin GPIO pin number this Input reads.
     * @param pullUp true = internal pull-up, false = internal pull-down.
     * @return Input* Newly heap-allocated, already-registered Input.
     */
    Input *NewInput(char *name, uint timeout, uint32_t uxStackDepth, UBaseType_t uxPriority, uint8_t pin, bool pullUp);

    /**
     * @brief Reads the GPIO pin's value directly from hardware - exact
     * at the moment of the call, never stale, since Input keeps no
     * cache of its own to fall behind.
     *
     * @param in Input to read.
     * @return bool Current pin value.
     */
    bool GetInputValue(Input *in);
#endif
