/**
 * @file state.c
 * @brief Placeholder run_state() implementations for the four concrete
 * states (idle, loop, sub, error). Most of this doesn't do anything
 * specific to the board's hardware on purpose - it exists to show the
 * state pattern's own mechanism (self/context, and how SetState() moves
 * from one state to the next). context->peripheral is already wired up
 * and ready to use (see NewContext() in context.c).
 *
 * One real example is wired in: StateLoop_Run() sends a fixed 6-byte
 * message over uart every cycle, then reads back whatever is already
 * available (see UartWrite()/UartRead() in uart.h) - non-blocking, so it
 * may print fewer than 6 bytes if the reply hasn't fully arrived yet.
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
    ThyoneI *u = (ThyoneI *)GetAgentByName("uart");
    if (u == NULL) {
        return;
    }

    uint8_t msg[512] = {0x00};
    uint8_t addr[4] = {0x5A, 0xF0, 0x00, 0x6C};
    uint8_t out[6] = {0x00};
    
    ThyoneISendToAddress(u, addr, msg, 512);
    ThyoneIRead(u, out, 6);
    for (int i=0;i<6;i++) printf("%d ", out[i]);
    printf("\n");
    ThyoneIRead(u, out, 6);
    for (int i=0;i<6;i++) printf("%d ", out[i]);
    printf("\n");
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
