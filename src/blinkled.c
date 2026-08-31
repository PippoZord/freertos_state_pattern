#include <stdlib.h>
#include "blinkled.h"

static void BlinkLed_Behave(Agent *self) {
    BlinkLed *led = (BlinkLed *)self;
    led->state = !led->state;
    gpio_put(led->pin, led->state);
}

static void Blinked_Delete(Agent *self) {
    BlinkLed *led = (BlinkLed *)self;
    led->state = 0;
    gpio_put(led->pin, 0);
}

BlinkLed *NewBlinkLed(char *name, int timeout, uint32_t uxStackDepth, UBaseType_t uxPriority, uint pin) {
    BlinkLed *led = malloc(sizeof(BlinkLed));
    led->pin = pin;
    led->state = false;
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    Agent_Init(&led->base, name, timeout, uxStackDepth, uxPriority, BlinkLed_Behave, Blinked_Delete);
    return led;
}
