#ifndef ADC_H
#define ADC_H
    #include "agent.h"
    #include "pico/stdlib.h"

    /**
     * @brief Sentinel for Adc_Init()/NewAdc()'s pin argument meaning
     * "this channel has no physical GPIO" - the internal temperature
     * sensor (ADC_TEMPERATURE_CHANNEL_NUM, see hardware/adc.h) is read
     * through the ADC like any other channel, but isn't wired to a pin
     * at all, so there is nothing for adc_gpio_init()/gpio_deinit() to
     * touch. See InternalTemperature (internaltemperature.h) for the
     * concrete use of this.
     */
    #define ADC_GPIO_NONE 0xFF

    /**
     * @brief An ADC input, as an Agent extension: base (Agent) plus the
     * GPIO pin (or ADC_GPIO_NONE) and ADC channel it reads. Like
     * Output/Pwm, keeps no cached value of its own - GetAdcValue()
     * always talks to the hardware directly. Note the ADC itself is a
     * single shared peripheral with several multiplexed input channels
     * (not one converter per pin), which is why every read re-selects
     * its own channel first (see GetAdcValue()).
     */
    typedef struct {
        Agent base;
        uint8_t gpio;
        uint8_t channel;
    } Adc;

    /**
     * @brief Allocates a new Adc and configures its GPIO for analog
     * input (skipped if pin == ADC_GPIO_NONE - see Adc_Init()).
     * Registers via Agent_Init() with timeout == 0: Adc_Behave() has
     * nothing to refresh, so no FreeRTOS task is needed for it.
     *
     * @param name Name of the task/agent (copied internally, see Agent_Init).
     * @param timeout Period in ms between behave() calls; use 0 (no task).
     * @param uxStackDepth Task stack depth, in words (moot if timeout == 0).
     * @param uxPriority FreeRTOS task priority (moot if timeout == 0).
     * @param pin GPIO pin number this Adc reads (26-29 on RP2040/RP2350),
     * or ADC_GPIO_NONE for a channel with no physical pin.
     * @param channel ADC input channel matching pin (0 for GPIO26, 1 for
     * GPIO27, 2 for GPIO28, 3 for GPIO29) - see adc_select_input().
     * @return Adc* Newly heap-allocated, already-configured Adc.
     */
    Adc *NewAdc(char *name, uint timeout, uint32_t uxStackDepth, UBaseType_t uxPriority, uint8_t pin, uint8_t channel);

    /**
     * @brief Configures adc's GPIO for analog input (unless pin ==
     * ADC_GPIO_NONE, in which case no GPIO is touched - used for
     * channels with no physical pin, e.g. the internal temperature
     * sensor: see InternalTemperature), then delegates to Agent_Init()
     * with the given behave/delete. Split out from NewAdc() so a type
     * that extends Adc (embedding it as its own first field) can reuse
     * this hardware setup while supplying its own behave()/delete(),
     * instead of duplicating it - the same reasoning behind
     * Output_Init() (see output.h).
     *
     * @param adc Adc to initialize (memory already allocated by the caller).
     * @param pin GPIO pin number this Adc reads, or ADC_GPIO_NONE.
     * @param channel ADC input channel matching pin.
     * @param behave Behaviour function to register with Agent_Init().
     * @param delete Delete function to register with Agent_Init().
     * @param name Name of the task/agent (copied internally, see Agent_Init).
     * @param timeout Period in ms between behave() calls; use 0 (no task).
     * @param stack Task stack depth, in words (moot if timeout == 0).
     * @param prio FreeRTOS task priority (moot if timeout == 0).
     */
    void Adc_Init(Adc *adc, uint8_t pin, uint8_t channel, AgentBehaviour behave, AgentDelete delete, char *name, uint timeout, uint32_t stack, UBaseType_t prio);

    /**
     * @brief Reads a fresh 12-bit sample from hardware - exact at the
     * moment of the call, never stale, since Adc keeps no cache of its
     * own to fall behind. Re-selects adc's own channel first, since the
     * ADC peripheral is shared across every channel (see the @brief on
     * the Adc struct).
     *
     * @param adc Adc to read.
     * @return uint16_t Raw sample, 0-4095 (12-bit).
     */
    uint16_t GetAdcValue(Adc *adc);

    /**
     * @brief Adc keeps no cache to refresh, so there is nothing
     * periodic to do; exists only because Agent_Init() requires a
     * non-NULL behave(). Exposed (not static) so a derived type with
     * nothing of its own to refresh either (e.g. InternalTemperature)
     * can reuse it directly instead of redefining an identical no-op.
     *
     * @param self The agent itself (unused).
     */
    void Adc_Behave(Agent *self);

    /**
     * @brief Releases adc's GPIO (unless it was configured with
     * ADC_GPIO_NONE, in which case there is nothing to release).
     * Exposed (not static) so a derived type with nothing extra of its
     * own to release (e.g. InternalTemperature) can reuse it directly
     * as its own AgentDelete, the same way NewAdc() does.
     *
     * @param self The Adc being deleted, as its base Agent.
     */
    void Adc_Delete(Agent *self);

#endif
