#include "peripheral.h"
#include <stdlib.h>

static Peripheral *istance = NULL;

Peripheral *GetPeripheral() {
    if (istance == NULL) {
        istance = malloc(sizeof(Peripheral));
        istance->agent = NewAgent("agent1", 0, 512, 1);
        istance->agent = NewAgent("agent2", 110, 512, 1);
    }
    return istance;
}
