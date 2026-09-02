#include "context.h"
#include "state.h"
#include <stdio.h>
#include "pico/time.h"
#include "runtime.h"
#include "peripheral.h"
#include "agent.h"
#include "input.h"

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

void Callback1(uint gpio, uint32_t events) {
    printf("FromCallback 1\n");
}


void Callback2(uint gpio, uint32_t events) {
    printf("FromCallback 2\n");
}

void StateLoop_Run(State *self, Context *context) {
    AddGPIOCallBack(1, &Callback1);
    AddGPIOCallBack(2, &Callback2);
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

