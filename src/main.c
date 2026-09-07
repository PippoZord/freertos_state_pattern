#include <stdio.h>
#include "pico/stdlib.h"
#include "context.h"
#include "FreeRTOS.h"
#include "task.h"
#include "peripheral.h"
#include "powman_sleep.h"

// How long the whole system (state machine, radio, ADC, the BlinkLed
// "active" indicator, ...) stays up and running normally before
// DeepSleepTask forces a POWMAN dormant sleep. There is no timed wake-up
// here (see WAKE_GPIO below) - the chip stays dormant until that GPIO
// fires, however long that takes.
#define ACTIVE_MS 10000

// GPIO8 doubles as ThyoneI's UART1 TX pin (see NewThyoneI() in
// peripheral.c) - a plain UART TX line idles HIGH (mark state) when
// nothing is being sent, which is exactly the state it's already in once
// DeepSleepTask fires. WAKE_GPIO_HIGH is therefore false (wake on a
// HIGH->LOW transition): the required "opposite level before sleeping"
// (see powman_gpio_wake_t in powman_sleep.h) is met for free, with no
// need to fight the UART peripheral's own idle drive. Ground this pin
// briefly to wake the chip. If instead you want to wake by driving GPIO8
// UP to 3V3, flip this to true - but then something needs to actively
// hold it LOW first, since UART TX won't do that on its own.
#define WAKE_GPIO 8u
#define WAKE_GPIO_HIGH false

/**
 * @brief One-shot FreeRTOS task, deliberately NOT an Agent: Agent's Run()
 * (see agent.h) calls behave() immediately and then loops on a fixed
 * period forever - there is no "wait once, then act a single time"
 * semantic in it, so a plain xTaskCreate() task is the right tool here.
 *
 * Waits ACTIVE_MS after the scheduler starts - i.e. after every real
 * Agent (BlinkLed, ThyoneI, InternalTemperature, the state machine's own
 * task, ...) has already been running normally for that long - then
 * forces the chip into POWMAN dormant mode until WAKE_GPIO fires (no
 * timer alarm involved). Every wake-up from dormant mode is a full chip
 * reboot back to main() - there is no "resume" path, see powman_sleep.h.
 */
static void DeepSleepTask(void *pvParameters) {
    (void)pvParameters;
    vTaskDelay(pdMS_TO_TICKS(ACTIVE_MS));

    printf("deep sleep: system was active for %dms, going dormant until GPIO%u goes %s...\n",
           ACTIVE_MS, WAKE_GPIO, WAKE_GPIO_HIGH ? "HIGH" : "LOW");
    sleep_ms(50); // let the printf actually drain out over USB before sleeping

    // abs_time_ms just seeds POWMAN's internal timer - only the relative
    // sleeping_ms passed to powman_off_for_ms() below matters here, this
    // isn't used as a real wall-clock reference.
    //
    // All four optional low-power steps enabled: POWMAN_STATE (see
    // powman_power_off() in powman_sleep.c) only powers down SWCORE/XIP/
    // SRAM - XOSC, ROSC, both PLLs and the USB PHY are NOT touched by that
    // alone and keep drawing current through the whole "dormant" period
    // unless stopped here explicitly. This is exactly what accounted for
    // going from ~100uA (reference deepsleep.c, all four enabled) to
    // ~600uA (these all false) on the same board/stock clock config - see
    // the @warning on each powman_stop_xosc()/powman_stop_rosc()/
    // powman_power_down_plls()/powman_isolate_usb_phy() in powman_sleep.h;
    // all four are safe here since this project never touches the clock
    // configuration away from the stock boot defaults either.
    // low_power_wifi_chip is a no-op on plain Pico 2 (no CYW43 to power
    // down) and harmless to leave on for when this is built for Pico 2 W.
    powman_init(1704067200000ull,
                /*low_power_xosc=*/true,
                /*low_power_rosc=*/true,
                /*low_power_plls=*/true,
                /*low_power_usb_phy=*/true,
                /*low_power_wifi_chip=*/true,
                /*exclude_gpios_mask=*/0);

    powman_off_until_gpio(WAKE_GPIO, WAKE_GPIO_HIGH, POWMAN_PWRUP0, NULL, 0);

    // Unreachable: powman_off_until_gpio() puts the chip in dormant mode
    // and every wake-up is a full chip reboot back to main().
    while (true) {
        tight_loop_contents();
    }
}

int main() {
    stdio_init_all();

    // Only meaningful if POWMAN previously powered SWCORE down (i.e. we
    // got here via a powman_off*() call before) - 0 on a fresh power-on
    // or a plain reset. Read before anything else touches POWMAN.
    uint32_t reason = powman_get_wakeup_reason();
    if (reason & POWMAN_WAKEUP_GPIO0) {
        printf("boot: woke up from POWMAN dormant mode (GPIO%u)\n", WAKE_GPIO);
    } else {
        printf("boot: fresh boot (or non-POWMAN reset)\n");
    }

    Context c = NewContext();
    xTaskCreate(RunCurrentState, "context runtime", 512, &c, 1, NULL);
    
    xTaskCreate(DeepSleepTask, "deep sleep", 512, NULL, 1, NULL);

    vTaskStartScheduler();
}
