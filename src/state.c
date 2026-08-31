#include "context.h"
#include "state.h"
#include <stdio.h>
#include "pico/time.h"


void StateIdle_Run(State *self, Context *context) {
    printf("RunIdle\n");
    SetState(context, STATE_LOOP);
}

void StateLoop_Run(State *self, Context *context) {
    printf("RunLoop\n");
    SetState(context, STATE_ERROR);
}


void StateError_Run(State *self, Context *context) {
    printf("RunError\n");
}

