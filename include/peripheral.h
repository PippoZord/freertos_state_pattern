#ifndef PERIPHERAL_H
#define PERIPHERAL_H
    #include "agent.h"
    #include "blinkled.h"
    #include "output.h"
    #include "input.h"

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
        BlinkLed *led;
        Output *out1;
        Input *in2;
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
#endif
