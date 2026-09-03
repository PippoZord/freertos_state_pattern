/**
 * @file state.c
 * @brief Placeholder run_state() implementations for the four concrete
 * states (idle, loop, sub, error). Most of this doesn't do anything
 * specific to the board's hardware on purpose - it exists to show the
 * state pattern's own mechanism (self/context, and how SetState() moves
 * from one state to the next). context->peripheral is already wired up
 * and ready to use (see NewContext() in context.c).
 *
 * One real example is wired in: in2's GPIO interrupt drives a state
 * transition to STATE_ERROR. in2CB() is the actual interrupt-facing
 * handler (called by Peripheral's dispatcher, OnInGPIOInterrupt() in
 * peripheral.c, itself running in real interrupt context) - it only
 * sets a flag, on purpose: interrupts should do as little as possible.
 * StateLoop_Run() checks that flag once per cycle, from the state
 * machine's own task, and only there calls SetState() - nothing about
 * the transition itself runs in interrupt context.
 */

#include "context.h"
#include "state.h"
#include "runtime.h"
#include "peripheral.h"
#include <stdio.h>
#include <stdbool.h>

// Written from interrupt context (in2CB), read from task context
// (StateLoop_Run). A single aligned bool read/write is atomic on
// Cortex-M, so this needs no critical section - unlike
// AddGPIOCallBack()'s multi-field table writes (see peripheral.c).
static volatile bool in2Triggered = false;

static void in2CB(uint gpio, uint32_t events) {
    (void)gpio;
    (void)events;
    in2Triggered = true;
}

void StateIdle_Run(State *self, Context *context) {
    SetState(context, STATE_LOOP);
}

void StateLoop_Run(State *self, Context *context) {
    
}

void StateError_Run(State *self, Context *context) {
    SetState(context, STATE_IDLE);
}

void SubState_Run(State *self, Context *context) {
    // downcast to access SubState's own field (value), not present on
    // the base State - see the @brief on SubState in context.h.
    SubState *s = (SubState *)(self);
    printf("SubState %d\n", s->value);
    SetState(context, STATE_ERROR);
}
