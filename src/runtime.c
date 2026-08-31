/**
 * @file runtime.c
 * @brief Implementation of the Runtime singleton: a growable table of
 * (FreeRTOS task handle, Agent*) pairs, with functions to start/stop
 * Agents and to look them up by name or by task handle. Freed slots
 * (handle == 0, agent == NULL) are reused before the table grows.
 * Every function below goes through GetIstance() rather than touching the
 * `istance` global directly, so the singleton is always lazily initialized
 * on first use — callers don't need to remember to call GetIstance() first.
 */

#include "runtime.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/** @brief Process-wide Runtime singleton; NULL until the first GetIstance() call. */
static Runtime *istance = NULL;


/** @copydoc GetIstance */
Runtime *GetIstance(){
    if (istance == NULL) {
        istance = malloc(sizeof(Runtime));
        if (istance == NULL) panic("ERROR: could not allocate Runtime");

        istance->tasks = malloc(sizeof(AgentTaskArray));
        if (istance->tasks == NULL) panic("ERROR: could not allocate AgentTaskArray");

        istance->tasks->tasks = malloc(sizeof(AgentTask)*2);
        if (istance->tasks->tasks == NULL) panic("ERROR: could not allocate initial task table");

        istance->tasks->tasks[0].handle=0;
        istance->tasks->tasks[1].handle=0;
        istance->tasks->tasks[0].agent = NULL;
        istance->tasks->tasks[1].agent = NULL;
        istance->tasks->size=2;
        istance->tasks->len=0;
    }
    return istance;
}


/** @copydoc getTaskByHandle */
AgentTask *getTaskByHandle(TaskHandle_t handle) {
    if (handle == 0) panic("ERROR: handle cannot be NULL");
    Runtime *rt = GetIstance();

    for (int i =0; i<rt->tasks->len; i++) {
        if (handle == rt->tasks->tasks[i].handle ) {
            return rt->tasks->tasks+i;
        }
    }
    return NULL;
}

/**
 * @brief Internal helper: find the AgentTask entry whose agent has the
 * given name. Used by DeleteAgent() to resolve an Agent* to its task
 * handle before deleting the FreeRTOS task, and by GetAgentByName() to
 * hand callers the Agent* alone.
 *
 * @param name Agent name to search for; cannot be NULL, panics otherwise.
 * @return AgentTask* Matching entry, or NULL if no running agent has that name.
 */
static AgentTask *getTaskByName(char *name) {
    if (name == NULL) panic("ERROR: name cannot be NULL");
    Runtime *rt = GetIstance();

    for (int i =0; i<rt->tasks->len; i++) {
        if (rt->tasks->tasks[i].agent != NULL &&
            strcmp(name, rt->tasks->tasks[i].agent->name) == 0) {
            return rt->tasks->tasks+i;
        };
    }
    return NULL;
}

/** @copydoc GetAgentByName */
Agent *GetAgentByName(char *name) {
    AgentTask *task = getTaskByName(name);
    return task == NULL ? NULL : task->agent;
}

/**
 * @brief Internal helper: double the capacity of the runtime's task
 * table in place (realloc). Called by addTask() when the table is full
 * and no free slot could be reused. Panics if the table is NULL or the
 * reallocation fails.
 */
static void reallocTasks(){
    Runtime *rt = GetIstance();
    if (rt->tasks->tasks == NULL) {
        panic("array cannot be null");
    }

    AgentTask *double_copy = realloc(rt->tasks->tasks, sizeof(AgentTask)*(rt->tasks->size*2));
    if (double_copy == NULL) {
        panic("error in realloc\n");
    }
    rt->tasks->tasks = double_copy;
    rt->tasks->size *= 2;
}

/**
 * @brief Internal helper: register a (handle, agent) pair in the task
 * table. Reuses the first free slot (handle == 0) if one exists;
 * otherwise appends at the end, growing the table via reallocTasks()
 * first if it's full. Called by StartAgent() right after xTaskCreate().
 *
 * @param agentTask Entry to copy into the table (the struct is copied by value); cannot be NULL, panics otherwise.
 */
static void addTask(AgentTask *agentTask) {
    if (agentTask == NULL) panic("ERROR: agentTask cannot be NULL");
    Runtime *rt = GetIstance();

    for (int i =0; i<rt->tasks->len; i++){
        if (rt->tasks->tasks[i].agent == NULL) {
            rt->tasks->tasks[i].agent = agentTask->agent;
            rt->tasks->tasks[i].handle = agentTask->handle;
            return;
        }
    }
    if (rt->tasks->len == rt->tasks->size) {
        reallocTasks();
    }
    rt->tasks->tasks[rt->tasks->len] = *agentTask;
    rt->tasks->len++;
}

/**
 * @brief Internal helper: free the Agent matching the given name (its
 * name buffer and the Agent struct itself) and clear its slot in the
 * task table (handle = 0, agent = NULL), making the slot reusable by
 * addTask(). Does NOT delete the FreeRTOS task; the caller (DeleteAgent())
 * is expected to have already called vTaskDelete() on its handle.
 *
 * @param name Name of the agent to remove; cannot be NULL, panics otherwise.
 */
static void removeTask(char *name) {
    if (name == NULL) panic("ERROR: name cannot be NULL");
    Runtime *rt = GetIstance();

     for (int i =0; i<rt->tasks->len; i++) {
        if (rt->tasks->tasks[i].agent != NULL &&
            strcmp(name, rt->tasks->tasks[i].agent->name) == 0) {
            free(rt->tasks->tasks[i].agent->name);
            free(rt->tasks->tasks[i].agent);
            rt->tasks->tasks[i].agent = NULL;
            rt->tasks->tasks[i].handle = 0;
            return;
        }
    }
}

/** @copydoc PrintRuntimeState */
void PrintRuntimeState() {
    Runtime *rt = GetIstance();
    printf("[RUNTIME] len=%d size=%d\n", rt->tasks->len, rt->tasks->size);
    for (int i = 0; i < rt->tasks->len; i++) {
        AgentTask *t = &rt->tasks->tasks[i];
        if (t->agent != NULL) {
            printf("  [%d] handle=%p  name=%s  timeout=%d  stack=%u  prio=%lu\n",
                   i, (void *)t->handle, t->agent->name, t->agent->timeout,
                   t->agent->uxStackDepth, (unsigned long)t->agent->uxPriority);
        } else {
            printf("  [%d] libero\n", i);
        }
    }
}

/** @copydoc StartAgent */
void StartAgent(Agent *agent) {
    if (agent == NULL) panic("ERROR: agent cannot be NULL");
    AgentTask task = { .handle = NULL, .agent = agent };
    if (agent->timeout > 0) {
        TaskHandle_t handle;
        BaseType_t created = xTaskCreate(Run, agent->name, agent->uxStackDepth, agent, agent->uxPriority, &handle);
        if (created != pdPASS) panic("ERROR: xTaskCreate failed for agent '%s'", agent->name);
        task.handle = handle;
    } 
    addTask(&task);
    printf("[RUNTIME] StartAgent('%s')\n", agent->name);
    PrintRuntimeState();

}

/** @copydoc DeleteAgent */
void DeleteAgent(Agent *agent) {
    if (agent == NULL) panic("ERROR: agent cannot be NULL");

    AgentTask *ag = getTaskByName(agent->name);
    if (ag == NULL) return;
    printf("[RUNTIME] DeleteAgent('%s')\n", agent->name);
    if (ag->handle != NULL) {
        vTaskDelete(ag->handle);
    }
    // Release the agent's hardware (e.g. turn off an LED) while agent is
    // still valid - removeTask() below frees agent->name and agent itself,
    // so agentDelete() must run before it, not after.
    agent->agentDelete(agent);
    removeTask(agent->name);
    PrintRuntimeState();
}
