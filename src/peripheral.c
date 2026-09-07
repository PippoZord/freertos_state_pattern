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

/**
 * @brief GPIO driving the "system is active" indicator LED (see led in
 * Peripheral, wired up in GetPeripheral()). Deliberately NOT
 * PICO_DEFAULT_LED_PIN: on a real Pico 2 W, the onboard LED is on the
 * CYW43 wireless chip, not a plain RP2350 GPIO - PICO_DEFAULT_LED_PIN
 * isn't even defined for the pico2_w board. GPIO2 is a plain, unused pin
 * safely outside the CYW43-reserved range (WL_REG_ON=23, WL_DATA=24,
 * WL_CS=25, WL_CLOCK=29) - wire an external LED (+ series resistor) to
 * it to see this indicator on Pico 2 W hardware.
 */
#define INDICATOR_LED_GPIO 2

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
        istance->u = NewThyoneI("uart", 10, 1024, 1, uart1, 8, 9, 115200);
        istance->temp = NewInternalTemperature("temp", 0, 512, 1);
        // Visible "system is active" indicator: blinks the whole time the
        // system is up and running normally, right up until DeepSleepTask
        // (see main.c) forces the chip into POWMAN dormant mode.
        istance->led = NewBlinkLed("led", 500, 256, 1, INDICATOR_LED_GPIO);
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
