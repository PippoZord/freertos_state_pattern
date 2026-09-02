#ifndef BLINKLED_H
#define BLINKLED_H

#include "agent.h"
#include "pico/stdlib.h"

/**
 * @brief A blinking LED, as an Agent extension: base (Agent) plus the
 * GPIO pin it drives and its current on/off state. Embeds Agent as its
 * first field (base) so a BlinkLed* can be upcast to Agent* (e.g.
 * &led->base) and, from inside its own behave()/delete(), downcast
 * back with (BlinkLed *)self - the same first-member idiom SubState
 * uses on State.
 */
typedef struct {
    Agent base;
    uint8_t pin;
    bool state;
} BlinkLed;

/**
 * @brief Allocates a new BlinkLed, configures its GPIO as an output and
 * registers it via Agent_Init() - its own behave() toggles the pin
 * every timeout ms, so unlike Output it does need a real task
 * (timeout > 0).
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
