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
     * Optionally also enables a GPIO interrupt on this pin: pass a
     * non-NULL callback to have it called on the events in
     * interruptEvents (an OR of GPIO_IRQ_EDGE_RISE/FALL/LEVEL_HIGH/LOW,
     * see hardware/gpio.h); pass NULL to leave the interrupt disabled.
     *
     * @warning gpio_set_irq_enabled_with_callback() sets ONE callback
     * shared by every GPIO interrupt on this core (see hardware/gpio.h:
     * "All GPIOs/events added in this way on the same core share the
     * same callback"). Creating a second interrupt-enabled Input
     * silently replaces this one's callback for every pin, not just its
     * own. Only one interrupt-enabled Input is supported at a time this
     * way; do not rely on more than one being live simultaneously.
     *
     * @param name Name of the task/agent (copied internally, see Agent_Init).
     * @param timeout Period in ms between behave() calls; use 0 (no task).
     * @param uxStackDepth Task stack depth, in words (moot if timeout == 0).
     * @param uxPriority FreeRTOS task priority (moot if timeout == 0).
     * @param pin GPIO pin number this Input reads.
     * @param pullUp true = internal pull-up, false = internal pull-down.
     * @param interruptEvents Which events raise the interrupt (OR of
     * GPIO_IRQ_*); ignored if callback is NULL.
     * @param callback Called on those events; NULL to disable the interrupt.
     * @return Input* Newly heap-allocated, already-registered Input.
     */
    Input *NewInput(char *name, uint timeout, uint32_t uxStackDepth, UBaseType_t uxPriority, uint8_t pin, bool pullUp, uint32_t interruptEvents, void (*callback)(uint gpio, uint32_t events));

    /**
     * @brief Configures in's GPIO (already allocated by the caller) -
     * pull resistor and optional interrupt included - then delegates to
     * Agent_Init() with the given behave/delete. Split out from
     * NewInput() so a type that extends Input (embedding it as its own
     * first field) can reuse this hardware setup while supplying its
     * own behave()/delete(), instead of duplicating it - the same
     * reasoning behind Output_Init() (see output.h). Same @warning as
     * NewInput() applies to interruptEvents/callback.
     *
     * @param in Input to initialize (memory already allocated by the caller).
     * @param pin GPIO pin number this Input reads.
     * @param pullUp true = internal pull-up, false = internal pull-down.
     * @param interruptEvents Which events raise the interrupt (OR of
     * GPIO_IRQ_*); ignored if callback is NULL.
     * @param callback Called on those events; NULL to disable the interrupt.
     * @param behave Behaviour function to register with Agent_Init().
     * @param delete Delete function to register with Agent_Init().
     * @param name Name of the task/agent (copied internally, see Agent_Init).
     * @param timeout Period in ms between behave() calls; use 0 (no task).
     * @param stack Task stack depth, in words (moot if timeout == 0).
     * @param prio FreeRTOS task priority (moot if timeout == 0).
     */
    void Input_Init(Input *in, uint8_t pin, bool pullUp, uint32_t interruptEvents, void (*callback)(uint gpio, uint32_t events), AgentBehaviour behave, AgentDelete delete, char *name, uint timeout, uint32_t stack, UBaseType_t prio);

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
