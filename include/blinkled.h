#ifndef BLINKLED_H
#define BLINKLED_H

#include "agent.h"
#include "pico/stdlib.h"

typedef struct {
    Agent base;  
    uint pin;
    bool state;
} BlinkLed;

BlinkLed *NewBlinkLed(char *name, int timeout, uint32_t uxStackDepth, UBaseType_t uxPriority, uint pin);

#endif
