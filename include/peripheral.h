#ifndef PERIPHERAL_H
#define PERIPHERAL_H
    #include "agent.h"
    typedef struct {
        Agent *agent;
    } Peripheral;

    Peripheral *GetPeripheral();
#endif