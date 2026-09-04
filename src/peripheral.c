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
        for (int i = 0; i < MAX_CALLBACK; i++)
            istance->gpiosCallback[i] = (gpioCallback){ .gpio = -1, .callback = NULL };
        // uart1 on gpio 8/9: pins 16/17 stay reserved for stdio's own
        // debug UART (PICO_DEFAULT_UART_TX/RX_PIN, see src/CMakeLists.txt),
        // so this uses a separate physical UART peripheral and pins.
        istance->u = NewThyoneI("uart", 100, 1024, 1, uart1, 8, 9, 115200, 6);
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
