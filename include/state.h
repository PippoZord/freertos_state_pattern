#ifndef STATE_H
#define STATE_H
#include "context.h"

/**
 * @brief function of idle state
 * 
 * @param self used for access to attributes of the state
 * @param context use to switch context
 */
void StateIdle_Run(State *self, Context *context);


/**
 * @brief function of loop state
 * 
 * @param self used for access to attributes of the state
 * @param context use to switch context
 */
void StateLoop_Run(State *self, Context *context);

/**
 * @brief function of error state
 * 
 * @param self used for access to attributes of the state
 * @param context use to switch context
 */
void StateError_Run(State *self, Context *context);

#endif
