/**
 * @file agent.c
 * @brief Implementation of the base Agent type: initialization with
 * parameter validation, the default factory (NewAgent) and the generic
 * task loop (Run) that dispatches to each agent's polymorphic behaviour.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "agent.h"
#include "pico/platform/panic.h"
#include "FreeRTOS.h"
#include "task.h"
#include "runtime.h"

/**
 * @brief Validates the parameters of Agent_Init()/NewAgent(); calls panic()
 * and does not return on an invalid value. uxPriority is unsigned, so only
 * the upper bound is checked (it can never be negative).
 *
 * @param agent Agent to initialize, cannot be NULL.
 * @param name Agent name, must be at least one character.
 * @param timeout Period in ms, must be > 0.
 * @param uxStackDepth Stack depth in words, must be > 0.
 * @param uxPriority FreeRTOS priority, must be in [0, configMAX_PRIORITIES-1].
 * @param behave Behaviour function, cannot be NULL.
 */
static void checkParameters(Agent *agent, char *name, int timeout, uint32_t uxStackDepth, UBaseType_t uxPriority, AgentBehaviour behave, AgentDelete delete){
    if (agent == NULL)
        panic("ERROR: agent cannot be NULL");
    else if (strlen(name) == 0)
        panic("ERROR: name must be at least one char");
    else if (timeout <0)
        panic("ERROR: timeout should be greater than -1");
    else if (uxStackDepth<1)
        panic("ERROR: uxStackDepth should be greater than 0");
    else if (uxPriority > configMAX_PRIORITIES-1)
        panic("ERROR: priority must be between 0 and configMAX_PRIORITIES - 1");
    else if (behave==NULL)
        panic("ERROR: behave function cannot be NULL");
    else if (delete==NULL)
        panic("ERROR: delete function cannot be NULL");
}

void Agent_Init(Agent *agent, char *name, uint timeout, uint32_t uxStackDepth, UBaseType_t uxPriority, AgentBehaviour behave, AgentDelete delete) {
    checkParameters(agent, name, timeout,  uxStackDepth,  uxPriority,  behave, delete);
    agent->name = malloc(strlen(name) + 1);
    if (agent->name == NULL) panic("ERROR: During creation of a new agent: %s", name);
    strcpy(agent->name, name);
    agent->timeout = timeout;
    agent->uxStackDepth=uxStackDepth;
    agent->uxPriority = uxPriority;
    agent->behave = behave;
    agent->agentDelete = delete;
    StartAgent(agent);
}

/**
 * @brief Default behaviour for a "base" Agent created via NewAgent(): does
 * nothing, it just exists/waits within its own Run() cycle. Derived types
 * (e.g. BlinkLed) pass their own behave() to Agent_Init() instead.
 *
 * @param self The agent itself (unused).
 */
static void Agent_DefaultBehave(Agent *self) {
    (void)self;
}


/**
 * @brief Default delete/cleanup for a "base" Agent created via NewAgent():
 * no hardware to release, so it's a no-op. Derived types (e.g. BlinkLed)
 * pass their own delete() to Agent_Init() instead, to actually turn off
 * their hardware (e.g. the LED) before the Agent itself is freed.
 *
 * @param self The agent itself (unused).
 */
static void Agent_DefaultDelete(Agent *self) {
    (void)self;
}

/** @copydoc NewAgent */
Agent *NewAgent(char *name, int timeout, uint32_t uxStackDepth, UBaseType_t uxPriority) {
    Agent *agent = malloc(sizeof(Agent));
    if (agent == NULL) panic("ERROR: During creation of a new agent: %s", name);
    Agent_Init(agent, name, timeout, uxStackDepth, uxPriority, Agent_DefaultBehave, Agent_DefaultDelete);
    return agent;
}

/** @copydoc PrintInfo */
void PrintInfo(Agent *agent) {
    if (agent == NULL) return;
    printf("\tNAME = %s\tTIMEOUT=%d\tSTACK=%lu\tPRIO=%lu\n",
           agent->name, agent->timeout, (unsigned long)agent->uxStackDepth, (unsigned long)agent->uxPriority);
}

/** @copydoc Run */
void Run(void *pvParameters) {
    Agent *agent = (Agent *)pvParameters;
    while (1) {
        agent->behave(agent);            // dispatch polimorfico
        vTaskDelay(pdMS_TO_TICKS(agent->timeout));
    }
}
