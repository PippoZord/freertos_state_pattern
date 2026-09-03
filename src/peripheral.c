/**
 * @file peripheral.c
 * @brief Implementation of the Peripheral singleton: creates and wires
 * up every device this board has, once, on the first GetPeripheral()
 * call.
 */

#include "peripheral.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include "hardware/gpio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "blinkled.h"
#include "togglebutton.h"


/** @brief Process-wide Peripheral singleton; NULL until the first GetPeripheral() call. */
static Peripheral *istance = NULL;

/** @copydoc OnInGPIOInterrupt */
void OnInGPIOInterrupt(uint gpio, uint32_t events) {
    for (int i = 0; i<MAX_CALLBACK; i++) {
        if (istance->gpiosCallback[i].gpio == gpio)
            istance->gpiosCallback[i].callback(gpio, events);
    }
}

/** @copydoc GetPeripheral */
Peripheral *GetPeripheral() {
    if (istance == NULL) {
        istance = malloc(sizeof(Peripheral));
        // Reset the callback table BEFORE creating any device: in2's
        // NewToggleButton() below registers itself into this table via
        // AddGPIOCallBack() as it's created, so resetting the table
        // afterward would wipe that registration out - which is exactly
        // what happened here before this comment existed.
        for (int i = 0; i < MAX_CALLBACK; i++)
            istance->gpiosCallback[i] = (gpioCallback){ .gpio = -1, .callback = NULL };
        // out1 (pin 1) powers the capacitive touch module - driven high
        // in StateIdle_Run - while in2 (pin 0) reads its signal output.
        // Must be two different pins: in2 was briefly also on pin 1,
        // which fought with out1 for the same GPIO - whichever
        // Input_Init() ran last reconfigured the pin back to input,
        // undoing out1's output drive and cutting power to the module.
        istance->out1 = NewOutput("out1", 0, 512,1,1);
        istance->fled = NewFadeLed("fled", 10, 512, 1, 14, 10);
        istance->in2 = NewToggleButton("in2", 0, true, GPIO_IRQ_EDGE_FALL, 512, 1);
    }
    return istance;
}

void AddGPIOCallBack(uint gpio,  void (*callback)(uint gpio, uint32_t events)){

    taskENTER_CRITICAL();
    for (int i = 0; i< MAX_CALLBACK; i++){
        if (istance->gpiosCallback[i].gpio == -1) {
            istance->gpiosCallback[i].gpio = gpio;
            istance->gpiosCallback[i].callback = callback;
            break;
        }
        if (istance->gpiosCallback[i].gpio == gpio){
            istance->gpiosCallback[i].callback = callback;
            break;
        }
    }
    taskEXIT_CRITICAL();
}
