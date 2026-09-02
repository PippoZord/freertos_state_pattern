#ifndef BLINKLED_H
#define BLINKLED_H

#include "agent.h"
#include "pico/stdlib.h"
#include "output.h"

/**
 * @brief A blinking LED, as an Output extension (which is itself an
 * Agent extension - Agent <- Output <- BlinkLed, a 3-level chain): base
 * (Output) plus the LED's own on/off state. Embeds Output as its first
 * field (base) so a BlinkLed* can be upcast to Output* (e.g. &led->base)
 * or, transitively, to Agent* (&led->base.base) - and, from inside its
 * own behave()/delete(), downcast back with (BlinkLed *)self - the same
 * first-member idiom SubState uses on State, chained one level deeper.
 * behave() reuses Output's own SetOutputValue() to toggle the pin,
 * rather than touching gpio_put() itself.
 */
typedef struct {
    Output base;
    bool state;    // BlinkLed's own blink state, not a generic pin cache
} BlinkLed;

/**
 * @brief Allocates a new BlinkLed and registers it via Output_Init()
 * (which itself configures the GPIO as an output and calls
 * Agent_Init()) - its own behave() toggles the pin every timeout ms,
 * so unlike Output it does need a real task (timeout > 0).
 *
 * @param name Name of the task/agent (copied internally, see Agent_Init).
 * @param timeout Period in ms between toggles.
 * @param uxStackDepth Task stack depth, in words.
 * @param uxPriority FreeRTOS task priority (0 .. configMAX_PRIORITIES-1).
 * @param pin GPIO pin number this LED is wired to.
 * @return BlinkLed* Newly heap-allocated, already-blinking BlinkLed.
 */
BlinkLed *NewBlinkLed(char *name, int timeout, uint32_t uxStackDepth, UBaseType_t uxPriority, uint8_t pin);

#endif
