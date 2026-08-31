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


/**
 * @brief function of sub state
 * The signature is the same as the other Run functions; internally,
 * self is downcast from State* to SubState* to reach fields that only
 * this state has and State does not, such as SubState's value field.
 *
 * @param self used for access to attributes of the state
 * @param context use to switch context
 */
void SubState_Run(State *self, Context *context);
#endif
