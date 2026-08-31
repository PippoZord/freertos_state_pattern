#include <stdio.h>
#include "pico/stdlib.h"
#include "context.h"
#include "FreeRTOS.h"
#include "task.h"

int main() {
    stdio_init_all();
    Context c = NewContext();
    xTaskCreate( RunCurrentState, "test_runtime", 512, &c, 1, NULL);
    vTaskStartScheduler();
}