/**
 * @file state.c
 * @brief Placeholder run_state() implementations for the four concrete
 * states (idle, loop, sub, error). These don't do anything specific to
 * this board's hardware on purpose - they exist to show the state
 * pattern's own mechanism (self/context, and how SetState() moves from
 * one state to the next, forming the idle -> loop -> sub -> error ->
 * idle cycle described in the README), not to exercise any particular
 * Agent/Peripheral. Replace these bodies with real logic - context->
 * peripheral is already wired up and ready to use (see NewContext() in
 * context.c) - once you're building an actual state machine here.
 */

#include "context.h"
#include "state.h"
#include <stdio.h>

void StateIdle_Run(State *self, Context *context) {
    printf("RunIdle\n");
    SetState(context, STATE_LOOP);
}

void StateLoop_Run(State *self, Context *context) {
    printf("RunLoop\n");
    SetState(context, STATE_SUB);
}

void StateError_Run(State *self, Context *context) {
    printf("RunError\n");
    SetState(context, STATE_IDLE);
}

void SubState_Run(State *self, Context *context) {
    // downcast to access SubState's own field (value), not present on
    // the base State - see the @brief on SubState in context.h.
    SubState *s = (SubState *)(self);
    printf("SubState %d\n", s->value);
    SetState(context, STATE_ERROR);
}
