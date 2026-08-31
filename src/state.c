#include "context.h"
#include "state.h"
#include <stdio.h>
#include "pico/time.h"
#include "peripheral.h"

void StateIdle_Run(State *self, Context *context) {
    printf("RunIdle\n");
    Peripheral *peripheral = GetPeripheral();
    printf("%s %d\n", peripheral->agent->name, peripheral->agent->timeout);
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
    //downcast to acce to value
    SubState *s = (SubState *)(self);
    printf("SubState %d\n", s->value);
    SetState(context, STATE_ERROR);
}

