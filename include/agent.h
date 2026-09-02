#include "FreeRTOS.h"

#ifndef AGENT_H
#define AGENT_H

/**
 * @brief This is the base object for the runtime. every object which is an Agent is accepted by runtime.
 * 
 */
typedef struct Agent Agent;

/**
 * @brief This is the function which is run from scheduler
 * 
 */
typedef void (*AgentBehaviour)(Agent *self);


typedef void (*AgentDelete)(Agent *self);


/**
 * @brief Object Agent wants all parameters to be accepted from FreeRTOS Kernel
 * 
 */
struct Agent {
    char *name;
    int timeout;
    uint32_t uxStackDepth;
    UBaseType_t uxPriority;
    AgentBehaviour behave;
    AgentDelete agentDelete;
};
/**
 * @brief Initialize an already-allocated Agent in place. Validates the
 * parameters (panics on invalid values), copies the name into a separately
 * allocated buffer, sets the fields of the FreeRTOS task that will run it,
 * and immediately registers the agent with the runtime via StartAgent()
 * (see runtime.h) — so the FreeRTOS task is created, and behave() may
 * start running, before this call returns.
 *
 * @warning Because the task can start running immediately, any
 * type-specific fields a derived type (e.g. BlinkLed) needs inside its own
 * behave() must be fully set up BEFORE calling Agent_Init(), not after.
 * @warning Requires the runtime singleton to already exist, i.e.
 * GetIstance() must have been called at least once before this.
 *
 * @param agent Agent to initialize (memory already allocated by the caller, cannot be NULL).
 * @param name Name of the task/agent; copied internally, the caller keeps ownership of the string passed in.
 * @param timeout Period in ms between successive calls to behave() (see Run()).
 * @param uxStackDepth Task stack depth, in words (as in xTaskCreate).
 * @param uxPriority FreeRTOS task priority (0 .. configMAX_PRIORITIES-1).
 * @param behave Behaviour function called on every Run() cycle.
 * @param delete Delete function called from runtime to reset value of the agent
 */
void Agent_Init(Agent *agent, char *name, uint timeout, uint32_t uxStackDepth, UBaseType_t uxPriority, AgentBehaviour behave, AgentDelete delete);

/**
 * @brief Allocate a new "base" Agent (default, no-op behaviour), initialize
 * it via Agent_Init() and start it (Agent_Init calls StartAgent()
 * internally, see runtime.h) — the returned agent is already running as a
 * FreeRTOS task. Used for generic agents; derived types (e.g. BlinkLed)
 * call Agent_Init() directly on their own base field with their own
 * behave(), after finishing their own setup (see the @warning on
 * Agent_Init).
 *
 * @param name Name of the task/agent (copied internally).
 * @param timeout Period in ms between successive calls to behave().
 * @param uxStackDepth Task stack depth, in words.
 * @param uxPriority FreeRTOS task priority (0 .. configMAX_PRIORITIES-1).
 * @return Agent* Newly heap-allocated, already-running agent (ownership to the caller, must be released via DeleteAgent()).
 */
Agent *NewAgent(char *name, int timeout, uint32_t uxStackDepth, UBaseType_t uxPriority);

/**
 * @brief Print the agent's main fields to stdout (name, timeout, stack,
 * priority). No-op if agent is NULL.
 *
 * @param agent Agent to print (may be NULL).
 */
void PrintInfo(Agent *agent);

/**
 * @brief Generic task loop used to run any Agent (or derived type) under
 * FreeRTOS: each cycle dispatches to agent->behave(), then sleeps for
 * agent->timeout milliseconds. This is the same function passed to
 * xTaskCreate() for both base Agents and derived types like BlinkLed: the
 * polymorphism lives in the behave function pointer, not in Run().
 *
 * @param pvParameters Pointer to an Agent (or a derived type whose first
 * field is an Agent), passed as pvParameters to xTaskCreate().
 */
void Run(void *pvParameters);

#endif
