#ifndef PERIPHERAL_H
#define PERIPHERAL_H
    #include "agent.h"
    #include "blinkled.h"
    #include "output.h"
    #include "input.h"
    #include "fadeled.h"
    #include "togglebutton.h"

    #define MAX_CALLBACK 4
    typedef struct {
        uint gpio;
        void (*callback)(uint gpio, uint32_t events);
    } gpioCallback;

    /**
     * @brief Singleton owning every piece of hardware this board uses,
     * one explicitly typed field per device (created once in
     * GetPeripheral()). Deliberately not a generic array of Agent*:
     * Runtime already provides that generic "look up any Agent by name"
     * path (see GetAgentByName()) for callers that want to treat any
     * agent uniformly - Peripheral's job is the opposite, to expose the
     * concrete, typed hardware this specific board has (e.g. out1->gpio)
     * without a cast at every use site.
     */
    typedef struct {
        gpioCallback gpiosCallback[MAX_CALLBACK];
        Output *out1;
        ToggleButton *in2;
        FadeLed *fled;
    } Peripheral;

    /**
     * @brief Get the process-wide Peripheral singleton, creating and
     * wiring up every device (as Agents, via NewBlinkLed()/NewOutput()/
     * NewInput()) on the first call.
     *
     * @return Peripheral* The singleton instance (never NULL).
     */
    Peripheral *GetPeripheral();
    void AddGPIOCallBack(uint gpio,  void (*callback)(uint gpio, uint32_t events));

    /**
     * @brief Peripheral's own GPIO interrupt dispatcher - the ONE GPIO
     * interrupt callback in effect for the whole core (see the
     * @warning on NewInput() in input.h). Any type that wants its pin's
     * interrupt routed through Peripheral's shared dispatch table
     * (AddGPIOCallBack()) instead of grabbing the SDK's single callback
     * slot for itself should pass THIS function as the callback to
     * Input_Init()/NewInput(), exactly as GetPeripheral() does for in2.
     *
     * @param gpio Which GPIO fired.
     * @param events Which event(s) fired (see GPIO_IRQ_* in hardware/gpio.h).
     */
    void OnInGPIOInterrupt(uint gpio, uint32_t events);
#endif
