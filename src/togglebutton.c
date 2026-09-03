/**
 * @file togglebutton.c
 * @brief Implementation of ToggleButton: a debounced push button whose
 * state flips once per accepted press. See the @brief on ToggleButton
 * in togglebutton.h for the design (timestamp debounce done directly
 * in interrupt context, multi-instance dispatch table).
 */

#include "togglebutton.h"
#include "peripheral.h"
#include "pico/time.h"
#include "hardware/gpio.h"
#include "pico/platform/panic.h"
#include <stdlib.h>
#include <stdio.h>

/** @brief Minimum time between two accepted presses; anything faster is bounce. */
#define TOGGLEBUTTON_DEBOUNCE_MS 100

/**
 * @brief Every ToggleButton currently alive, indexed by nothing in
 * particular - ToggleButton_OnInterrupt() finds the right one by
 * scanning for a matching ->input.gpio, the same pattern Peripheral
 * itself uses for gpiosCallback[]. A free slot has entry == NULL.
 */
static ToggleButton *instances[MAX_TOGGLEBUTTON_INSTANCES];

/**
 * @brief No cache to refresh, no polling needed: the toggle happens
 * directly in ToggleButton_OnInterrupt(). Exists only because
 * Agent_Init() requires a non-NULL behave().
 *
 * @param self The agent itself (unused).
 */
static void ToggleButton_Behave(Agent *self) {
    (void)self;
}

/**
 * @brief Releases the button's instance slot and pin before it's
 * freed, same cleanup Input_Delete does (see input.c) - ToggleButton
 * doesn't add any hardware of its own beyond the Input it extends.
 *
 * @param self The ToggleButton being deleted, as its base Agent.
 */
static void ToggleButton_Delete(Agent *self) {
    ToggleButton *btn = (ToggleButton *)self;
    for (int i = 0; i < MAX_TOGGLEBUTTON_INSTANCES; i++) {
        if (instances[i] == btn) {
            instances[i] = NULL;
            break;
        }
    }
    // Disable the interrupt before releasing the pin: gpio_deinit()
    // alone doesn't touch the interrupt-enable register, and every
    // ToggleButton always has one armed (see input.h's warning on
    // Input_Delete for what a floating pin with a still-armed
    // interrupt can do).
    gpio_set_irq_enabled(btn->input.gpio, GPIO_IRQ_LEVEL_LOW | GPIO_IRQ_LEVEL_HIGH | GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, false);
    gpio_disable_pulls(btn->input.gpio);
    gpio_deinit(btn->input.gpio);
}

/**
 * @brief Real interrupt-context handler, registered with
 * AddGPIOCallBack() so Peripheral's dispatcher (OnInGPIOInterrupt())
 * calls it when any ToggleButton's gpio fires. Looks up which
 * instance owns this gpio, then debounces by timestamp: a trigger
 * less than TOGGLEBUTTON_DEBOUNCE_MS after that instance's last
 * accepted one is bounce, ignored outright - only then does it flip
 * state and record the new timestamp.
 *
 * @param gpio Which GPIO fired.
 * @param events Which event(s) fired (unused: any registered edge counts).
 */
static void ToggleButton_OnInterrupt(uint gpio, uint32_t events) {
    (void)events;
    ToggleButton *btn = NULL;
    for (int i = 0; i < MAX_TOGGLEBUTTON_INSTANCES; i++) {
        if (instances[i] != NULL && instances[i]->input.gpio == gpio) {
            btn = instances[i];
            break;
        }
    }
    if (btn == NULL) {
        return;
    }
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - btn->lastTriggerMs < TOGGLEBUTTON_DEBOUNCE_MS) {
        return; // bounce from the same physical press, not a new one
    }
    btn->lastTriggerMs = now;
    btn->state = !btn->state;
}

/** @copydoc NewToggleButton */
ToggleButton *NewToggleButton(char *name, uint8_t pin, bool pullUp, uint32_t interruptEvents, uint32_t uxStackDepth, UBaseType_t uxPriority) {
    ToggleButton *btn = malloc(sizeof(ToggleButton));
    btn->state = false;
    btn->lastTriggerMs = 0;

    int slot = -1;
    for (int i = 0; i < MAX_TOGGLEBUTTON_INSTANCES; i++) {
        if (instances[i] == NULL) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        panic("ERROR: cannot create ToggleButton '%s': MAX_TOGGLEBUTTON_INSTANCES (%d) already exist", name, MAX_TOGGLEBUTTON_INSTANCES);
    }
    instances[slot] = btn;

    // callback = OnInGPIOInterrupt, not ToggleButton_OnInterrupt: this
    // pin's raw SDK interrupt is routed through Peripheral's shared
    // dispatcher, which then calls ToggleButton_OnInterrupt via the
    // AddGPIOCallBack() registration below - see the @brief on
    // OnInGPIOInterrupt in peripheral.h for why.
    Input_Init(&btn->input, pin, pullUp, interruptEvents, OnInGPIOInterrupt,
               ToggleButton_Behave, ToggleButton_Delete, name, 0, uxStackDepth, uxPriority);
    AddGPIOCallBack(pin, ToggleButton_OnInterrupt);
    return btn;
}

/** @copydoc GetToggleButtonState */
bool GetToggleButtonState(ToggleButton *btn) {
    return btn->state;
}
