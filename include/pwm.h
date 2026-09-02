#ifndef PWM_H
#define PWM_H
    #include "agent.h"

    /**
     * @brief A PWM output, as an Agent extension: base (Agent) plus the
     * GPIO pin it drives. Like Output/Input, keeps no cached level -
     * Get/SetPwmValue always talk to the hardware directly.
     */
    typedef struct{
        Agent base;
        uint8_t gpio;
    } Pwm;

    /**
     * @brief Allocates a new Pwm, routes its GPIO to the PWM peripheral
     * and starts its slice running (8-bit resolution: wrap = 255, so
     * SetPwmValue's useful range is 0-255 even though its parameter is
     * wider). Registers via Agent_Init() with timeout == 0: PwmBehave()
     * has nothing to refresh, so no FreeRTOS task is needed for it.
     *
     * @param name Name of the task/agent (copied internally, see Agent_Init).
     * @param timeout Period in ms between behave() calls; use 0 (no task).
     * @param uxStackDepth Task stack depth, in words (moot if timeout == 0).
     * @param uxPriority FreeRTOS task priority (moot if timeout == 0).
     * @param pin GPIO pin number this Pwm drives.
     * @return Pwm* Newly heap-allocated, already-running Pwm.
     */
    Pwm *NewPwm(char *name, uint timeout, uint32_t uxStackDepth, UBaseType_t uxPriority, uint8_t pin);

    /**
     * @brief Reads the channel's current compare level directly from
     * hardware - exact at the moment of the call, never stale, since
     * Pwm keeps no cache of its own to fall behind.
     *
     * @param pwm Pwm to read.
     * @return uint16_t Current compare level (0-255, given the fixed wrap).
     */
    uint16_t GetPwmValue(Pwm *pwm);

    /**
     * @brief Sets the channel's compare level directly on hardware.
     *
     * @param pwm Pwm to write to.
     * @param level New compare level (0-255, given the fixed wrap).
     */
    void SetPwmValue(Pwm *pwm, uint16_t level);
#endif
