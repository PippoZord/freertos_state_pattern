#include "peripheral.h"
#include <stdlib.h>

static Peripheral *istance = NULL;

Peripheral *GetPeripheral() {
    if (istance == NULL) {
        istance = malloc(sizeof(Peripheral));
        istance->agent = NewAgent("agent1", 1000, 512, 1);
    }
    return istance;
}
