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

#include "context.h"
#include "state.h"
#include "runtime.h"
#include "peripheral.h"
#include <stdio.h>
#include <stdbool.h>

void StateIdle_Run(State *self, Context *context) {
    SetState(context, STATE_LOOP);
}

void StateLoop_Run(State *self, Context *context) {
    Uart *u = (Uart *)GetAgentByName("uart");
    if (u == NULL) {
        return;
    }

    uint8_t out[] = {0x02, 0x06, 0x01, 0x00, 0x00, 0x05};
    UartWrite(u, out, sizeof(out));

    uint8_t in[12];
    uint n = UartRead(u, in, sizeof(in));
    if (n > 0) {
        printf("uart rx (%u bytes):", n);
        for (uint i = 0; i < n; i++) {
            printf(" %02x", in[i]);
        }
        printf("\n");
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
