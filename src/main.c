#include <stdio.h>
#include "pico/stdlib.h"
#include "context.h"

int main() {
    stdio_init_all();
    Context c = NewContext();
    RunCurrentState(&c);
}