#include "agent.h"
#include "FreeRTOS.h"
#include "task.h"

/**
 * @file runtime.h
 * @brief Main core of the library which manages the Agents. It owns a
 * single Runtime instance (singleton, see GetIstance()) that tracks every
 * Agent currently running: every Agent can be started (StartAgent) and
 * stopped (DeleteAgent) through it.
 */
#ifndef RUNTIME_H
#define RUNTIME_H

    /**
     * @brief Binds a running Agent to the FreeRTOS task that executes it,
     * so the runtime can look it up and stop it later (vTaskDelete on
     * handle). A slot with handle == 0 and agent == NULL is considered
     * free and can be reused by addtask().
     */
    typedef struct {
        TaskHandle_t handle;
        Agent *agent;
    } AgentTask;

    /**
     * @brief Growable array of AgentTask slots: the backing storage for
     * the Runtime's task table. len is the number of slots in use
     * (including freed-but-not-yet-reused ones), size is the allocated
     * capacity; it doubles (see realloctasks() in runtime.c) when full.
     */
    typedef struct {
        AgentTask *tasks;
        int len;
        int size;
    } AgentTaskArray;

    /**
     * @brief Singleton holding the table of all Agents currently managed
     * by the runtime. Access it via GetIstance(), which lazily allocates
     * it on first call.
     */
    typedef struct {
        AgentTaskArray *tasks;
    } Runtime;

    /**
     * @brief Get the process-wide Runtime singleton, allocating and
     * lazily initializing it (with an initial 2-slot task table) on the
     * first call.
     *
     * @return Runtime* The singleton instance (never NULL).
     */
    Runtime *GetIstance();

    /**
     * @brief Start an Agent: creates the FreeRTOS task that runs it
     * (via Run(), see agent.h) using the agent's own stack depth and
     * priority, and registers the resulting task handle in the runtime's
     * task table.
     *
     * @param agent Agent to start (must already be initialized, e.g. via NewAgent()); cannot be NULL, panics otherwise.
     */
    void StartAgent(Agent *agent);

    /**
     * @brief Look up the AgentTask entry for a given FreeRTOS task handle.
     *
     * @param handle FreeRTOS task handle to search for; cannot be NULL, panics otherwise.
     * @return AgentTask* Matching entry, or NULL if not found.
     */
    AgentTask *getTaskByHandle(TaskHandle_t handle);

    /**
     * @brief Stop an Agent: deletes its FreeRTOS task (vTaskDelete) and
     * frees the Agent itself (name buffer + struct), releasing its slot
     * in the task table for reuse. No-op if no running agent matches
     * agent->name.
     *
     * @param agent Agent to stop; cannot be NULL, panics otherwise. Treat the pointer as dangling after this call.
     */
    void DeleteAgent(Agent *agent);

    /**
     * @brief Debug dump to stdout of the runtime's task table: one line
     * per slot, showing either the agent's info or that the slot is free.
     */
    void PrintRuntimeState();

#endif