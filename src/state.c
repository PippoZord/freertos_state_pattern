/**
 * @file state.c
 * @brief Placeholder run_state() implementations for the four concrete
 * states (idle, loop, sub, error). Most of this doesn't do anything
 * specific to the board's hardware on purpose - it exists to show the
 * state pattern's own mechanism (self/context, and how SetState() moves
 * from one state to the next). context->peripheral is already wired up
 * and ready to use (see NewContext() in context.c).
 *
 * One real example is wired in: StateLoop_Run() reads in2 (a
 * ToggleButton) every cycle and prints its current state. Unlike an
 * earlier version of this file, no GPIO interrupt handling happens
 * here at all - ToggleButton wires its own interrupt straight into
 * Peripheral's dispatcher and debounces internally (see togglebutton.c)
 * - state.c only ever sees the already-debounced result via
 * GetToggleButtonState().
 */

#include "context.h"
#include "state.h"
#include "runtime.h"
#include "peripheral.h"
#include <stdio.h>
#include <stdbool.h>
#include "togglebutton.h"

void StateIdle_Run(State *self, Context *context) {
    SetOutputValue((Output *)GetAgentByName("out1"), 1);
    SetState(context, STATE_LOOP);
}

void StateLoop_Run(State *self, Context *context) {
    ToggleButton *tb = (ToggleButton *)GetAgentByName("in2");
    if (tb != NULL) {
        printf("%d\n", GetToggleButtonState(tb));
    }
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
