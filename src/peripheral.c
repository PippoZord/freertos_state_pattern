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


/** @brief Process-wide Peripheral singleton; NULL until the first GetPeripheral() call. */
static Peripheral *istance = NULL;

/**
 * @brief in2's interrupt callback, passed straight to NewInput(). Per
 * the @warning on NewInput (see input.h), this is the ONE GPIO
 * interrupt callback in effect for the whole core for as long as in2
 * exists - creating another interrupt-enabled Input would replace it.
 *
 * @param gpio Which GPIO fired.
 * @param events Which event(s) fired (see GPIO_IRQ_* in hardware/gpio.h).
 */
static void OnInGPIOInterrupt(uint gpio, uint32_t events) {
    for (int i = 0; i<MAX_CALLBACK; i++) {
        if (istance->gpiosCallback[i].gpio == gpio)
            istance->gpiosCallback[i].callback(gpio, events);
    }
}

/** @copydoc GetPeripheral */
Peripheral *GetPeripheral() {
    if (istance == NULL) {
        istance = malloc(sizeof(Peripheral));
        //istance->agent = NewAgent("agent1", 0, 512, 1);
        istance->out1 = NewOutput("out1", 0, 512,1, 1);
        istance->led = NewBlinkLed("led", 100, 512,1, 25);
        istance->in2 = NewInput("in2", 0, 512, 1, 0, true, GPIO_IRQ_EDGE_RISE, OnInGPIOInterrupt);
        for (int i = 0; i < MAX_CALLBACK; i++)
            istance->gpiosCallback[i] = (gpioCallback){ .gpio = -1, .callback = NULL };
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
