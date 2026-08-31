#include <stddef.h>
#include <stdio.h>
#include "context.h"
#include "state.h"
#include "pico/time.h"

Context NewContext() {
    Context ctx = {
        .idle = {.run_state =  StateIdle_Run},
        .loop = {.run_state =  StateLoop_Run},
        .error = {.run_state =  StateError_Run},
        .current_state = NULL
    };
    return ctx;
}


void RunCurrentState(Context *ctx) {
    /* Computed here, on the caller's real ctx, not inside NewContext(): a
     * pointer to a field of a by-value struct taken before it is returned
     * would still point at the temporary's stack slot after the copy. */
    ctx->current_state = &ctx->idle;
    while (1) {
        ctx->current_state->run_state(
            ctx->current_state,
            ctx
        );
        sleep_ms(3000);
    }
    
}

void SetState(Context *ctx, StateId id) {
    switch (id) {
        case STATE_IDLE:
            ctx->current_state = &ctx->idle;
            break;
        case STATE_LOOP:
            ctx->current_state = &ctx->loop;
            break;
        case STATE_ERROR:
            ctx->current_state = &ctx->error;
            break;
        default:
            break;
    }
}
