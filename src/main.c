#include <stdio.h>
#include "pico/stdlib.h"
#include "context.h"
#include "FreeRTOS.h"
#include "task.h"
#include "peripheral.h"

int main() {
    stdio_init_all();
    Peripheral *peripheral = GetPeripheral();
    Context c = NewContext();
    xTaskCreate( RunCurrentState, "context runtime", 512, &c, 1, NULL);
    vTaskStartScheduler();
}