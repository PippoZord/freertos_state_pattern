#include "context.h"
#include "state.h"
#include <stdio.h>
#include "pico/time.h"
#include "runtime.h"
#include "peripheral.h"
#include "agent.h"
#include "output.h"

void StateIdle_Run(State *self, Context *context) {
    printf("RunIdle\n");
    Agent *agent = GetAgentByName("agent1");
    if (agent == NULL) {
        printf("Non c'è Agent\n");
        SetState(context, STATE_LOOP);
        return;
    }
    printf("%s %d\n", agent->name, agent->timeout);
    SetState(context, STATE_LOOP);
}

void StateLoop_Run(State *self, Context *context) {
    // Manual blink test for out1: toggle it, then read it straight back
    // from hardware to prove Get/Set always agree, with no cache
    // involved on either side. Stays in STATE_LOOP on purpose for now
    // (transition below commented out) so this runs every cycle.
    printf("RunLoop");
    SetOutputValue(context->peripheral->out1, !GetOutputValue(context->peripheral->out1));
    printf("%d\n", GetOutputValue(context->peripheral->out1));
    //SetState(context, STATE_SUB);
}


void StateError_Run(State *self, Context *context) {
    printf("RunError\n");
    Agent *agent = GetAgentByName("led");
    if (agent != NULL) {
        DeleteAgent(agent);
    }
    SetState(context, STATE_IDLE);
}


void SubState_Run(State *self, Context *context) {
    //downcast to acce to value
    SubState *s = (SubState *)(self);
    printf("SubState %d\n", s->value);
    SetState(context, STATE_ERROR);
}

