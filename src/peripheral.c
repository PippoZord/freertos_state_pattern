#include "peripheral.h"
#include <stdlib.h>
#include "blinkled.h"

static Peripheral *istance = NULL;

Peripheral *GetPeripheral() {
    if (istance == NULL) {
        istance = malloc(sizeof(Peripheral));
        istance->agent = NewAgent("agent1", 0, 512, 1);
        istance->led  = NewBlinkLed("led", 100, 512, 1, 25);
    }
    return istance;
}
