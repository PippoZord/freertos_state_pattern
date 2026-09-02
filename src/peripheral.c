/**
 * @file peripheral.c
 * @brief Implementation of the Peripheral singleton: creates and wires
 * up every device this board has, once, on the first GetPeripheral()
 * call.
 */

#include "peripheral.h"
#include <stdlib.h>
#include "blinkled.h"
#include "output.h"

/** @brief Process-wide Peripheral singleton; NULL until the first GetPeripheral() call. */
static Peripheral *istance = NULL;

/** @copydoc GetPeripheral */
Peripheral *GetPeripheral() {
    if (istance == NULL) {
        istance = malloc(sizeof(Peripheral));
        istance->agent = NewAgent("agent1", 0, 512, 1);
        istance->led  = NewBlinkLed("led", 100, 512, 1, 10);
        istance->out1 = NewOutput("out1", 0, 512, 1, 25);
    }
    return istance;
}
