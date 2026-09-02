#ifndef OUTPUT_H
#define OUTPUT_H
    #include "agent.h"
    #include "stdbool.h"

    /**
     * @brief A digital GPIO output, as an Agent extension: base (Agent)
     * plus the GPIO pin number it drives. Deliberately has no cached
     * value field - the pin is only ever written by this program, so
     * Get()/Set() always go straight to the hardware and are already
     * exact; a cache here would just be one more thing that could drift
     * out of sync with reality for no benefit.
     */
    typedef struct {
        Agent base;
        uint8_t gpio;
    }Output;

    /**
     * @brief Allocates a new Output, configures its GPIO as an output
     * (driven low) and registers it via Agent_Init(). Pass timeout == 0:
     * Output_Behave() has nothing to do (no cache to refresh), so no
     * FreeRTOS task is needed for it.
     *
     * @param name Name of the task/agent (copied internally, see Agent_Init).
     * @param timeout Period in ms between behave() calls; use 0 (no task).
     * @param uxStackDepth Task stack depth, in words (moot if timeout == 0).
     * @param uxPriority FreeRTOS task priority (moot if timeout == 0).
     * @param pin GPIO pin number this Output drives.
     * @return Output* Newly heap-allocated, already-registered Output.
     */
    Output *NewOutput(char *name, uint timeout, uint32_t uxStackDepth, UBaseType_t uxPriority, uint8_t pin);

    /**
     * @brief Drives the GPIO pin high or low.
     *
     * @param out Output to write to.
     * @param value true = high, false = low.
     */
    void SetOutputValue(Output *out, bool value);

    /**
     * @brief Reads the GPIO pin's value directly from hardware - exact
     * at the moment of the call, never stale, since Output keeps no
     * cache of its own to fall behind.
     *
     * @param out Output to read.
     * @return bool Current pin value.
     */
    bool GetOutputValue(Output *out);
#endif
