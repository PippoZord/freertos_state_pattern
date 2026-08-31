#ifndef PERIPHERAL_H
#define PERIPHERAL_H
    #include "agent.h"
    #include "blinkled.h"
    typedef struct {
        Agent *agent;
        BlinkLed *led;
    } Peripheral;

    Peripheral *GetPeripheral();
#endif