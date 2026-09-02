#ifndef CONTEXT_H
#define CONTEXT_H
#include "peripheral.h"
// Forward declarations of the Context and State types, needed because
// struct State below refers to Context only by pointer (see state.h).
typedef struct Context Context;
typedef struct State State;
typedef struct SubState SubState;

// Identifies each concrete state, used by SetState() to pick which one
// ctx->current_state should point to.
typedef enum {
    STATE_IDLE,
    STATE_LOOP,
    STATE_SUB,
    STATE_ERROR
} StateId;

// A state: just the function to run while it is the active one.
struct State {
    // Behavior for this state; called once per RunCurrentState() iteration.
    void (*run_state)(State *self, Context *context);
};

/**
 * @brief A Substate: a state which extends the State struct.
 * .value is avaible in SubState variable but not in State
 * .super is the State object which context can use to preserve type and run current state
 * 
 */
struct SubState {
    State super;
    int value;
};

// Owns one State instance per concrete state, plus a pointer to whichever
// one is currently active.
struct Context {
    State idle;
    State loop;
    SubState sub;
    State error;
    // The board's hardware (Agents), so any State can reach it without
    // going through GetPeripheral() itself - set once in NewContext().
    Peripheral *peripheral;
    State *current_state;
};

/**
 * @brief Creates a Context for the state machine, wired up so each State's
 * run_state points at its behavior function and current_state starts at idle.
 *
 * @return A ready-to-use Context.
 */
Context NewContext();

/**
 * @brief Runs the state currently pointed to by ctx->current_state, forever.
 * Signature matches TaskFunction_t so this can be passed directly to
 * xTaskCreate(); pvParameters must be the Context* to run.
 *
 * @param pvParameters Context whose current state should run, as void*.
 */
void RunCurrentState(void *pvParameters);

/**
 * @brief Switches the active state.
 * Assigns to ctx->current_state a pointer to the State inside *ctx
 * (idle, loop, or error) that corresponds to the given id.
 *
 * @param ctx Context to update.
 * @param id Identifier of the state to switch to.
 */
void SetState(Context *ctx, StateId id);

#endif
