/**
 * @file state.c
 * @brief Placeholder run_state() implementations for the four concrete
 * states (idle, loop, sub, error). Most of this doesn't do anything
 * specific to the board's hardware on purpose - it exists to show the
 * state pattern's own mechanism (self/context, and how SetState() moves
 * from one state to the next). context->peripheral is already wired up
 * and ready to use (see NewContext() in context.c).
 *
 * One real example is wired in: StateLoop_Run() sends a 1-byte message
 * to a fixed Thyone-I address every cycle (see ThyoneISendToAddress()
 * in thyonei.h) and prints whether it was sent and confirmed - that
 * confirmation is the fixed CMD_DATA_CNF/CMD_TXCOMPLETE_RSP pair every
 * send gets, already waited for and checked inside
 * ThyoneISendToAddress(), nothing to read back here. It also drains
 * ThyoneIReceive() every cycle to print any message that arrived from
 * another device - those arrive on their own, via ThyoneI's own
 * background task, independently of whether/when we're sending. It
 * also reads the Adc wired to GPIO26 (see GetAdcValue() in adc.h) every
 * cycle and prints the raw 12-bit sample, and the chip's internal
 * temperature sensor (see GetInternalTemperatureCelsius() in
 * internaltemperature.h).
 */

#include <stdio.h>
#include <stdbool.h>
#include "context.h"
#include "state.h"
#include "runtime.h"
#include "peripheral.h"

void StateIdle_Run(State *self, Context *context) {
    SetState(context, STATE_LOOP);
}

void StateLoop_Run(State *self, Context *context) {


    InternalTemperature *temp = (InternalTemperature *)GetAgentByName("temp");
    if (temp != NULL) {
        printf("internal temperature = %.2f C\n", GetInternalTemperatureCelsius(temp));
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
