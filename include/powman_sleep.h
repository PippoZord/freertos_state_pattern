/*
 * powman_sleep.h — RP2350 POWMAN deep-sleep driver.
 *
 * C port of a MicroPython module that drives POWMAN directly via register
 * pokes (mem32-style) to force the chip into dormant mode (SWCORE/XIP/SRAM
 * powered down) and wake it back up on a timer alarm and/or up to 4 GPIOs.
 * Every wake-up is a full chip reboot — there is no "resume" path, the
 * program restarts from main() and clocks are reinitialized from scratch by
 * the boot ROM.
 *
 * Tested target: Pico 2 / RP2350, stock boot clock configuration.
 */

#ifndef POWMAN_SLEEP_H
#define POWMAN_SLEEP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- PWRUP slot identifiers ------------------------------------------- */
/* Register offsets, used directly as the "slot" argument below — mirrors
 * the Python module using PWRUP0..PWRUP3 constants as slot identifiers. */
#define POWMAN_PWRUP0 0x8Cu
#define POWMAN_PWRUP1 0x90u
#define POWMAN_PWRUP2 0x94u
#define POWMAN_PWRUP3 0x98u

/* ---- Wake-up reason bitmask (see powman_get_wakeup_reason) ------------ */
#define POWMAN_WAKEUP_CHIP_RESET 0x01u
#define POWMAN_WAKEUP_GPIO0      0x02u
#define POWMAN_WAKEUP_GPIO1      0x04u
#define POWMAN_WAKEUP_GPIO2      0x08u
#define POWMAN_WAKEUP_GPIO3      0x10u
#define POWMAN_WAKEUP_ALARM      0x40u

/* CYW43439 wireless chip WL_REG_ON pin (Pico 2 W only). */
#define POWMAN_CYW43_WL_REG_ON 23u

/* One GPIO wake-up condition: wake when `gpio` reaches level `high`.
 * IMPORTANT: the GPIO must already be at the OPPOSITE level before sleeping
 * — POWMAN wakes on a level transition, not on a static level. If high is
 * true, the GPIO must be LOW before sleep (then goes HIGH -> wake); if
 * high is false, it must be HIGH before sleep (then goes LOW -> wake).
 * Sleeping while the GPIO is already at the wake level prevents wake-up. */
typedef struct {
    uint8_t gpio;
    bool high;
} powman_gpio_wake_t;

/* Optional hook run last, right before __wfi() — after the alarm/GPIOs are
 * armed and after the optional low-power steps configured via
 * powman_init() have been applied. May be NULL. */
typedef void (*powman_before_sleep_t)(void);

/*
 * Initialize the POWMAN always-on timer and set its absolute time (ms).
 * abs_time_ms must be > 0.
 *
 * lowPowerXosc/Rosc/Plls/UsbPhy/WifiChip enable the optional extra
 * power-saving steps documented below (powman_stop_xosc/stop_rosc/
 * power_down_plls/isolate_usb_phy/power_down_wifi_chip) — enabling them
 * here makes every subsequent powman_off*() call apply them automatically,
 * without a manual before_sleep callback. low_power_plls requires
 * low_power_xosc=true (or a prior powman_stop_xosc() call) — see
 * powman_power_down_plls() below. low_power_xosc is applied immediately by
 * this call (safe to do early); the rest are deferred and applied last,
 * right before __wfi(), by every powman_off*() call.
 *
 * exclude_gpios_mask is a bitmask (bit N = GPIO N) of GPIOs that the
 * automatic GP0-22 force-low pass (see powman internals) should always
 * leave alone, on every subsequent powman_off*() call — on top of whichever
 * mask each individual call also passes.
 */
void powman_init(uint64_t abs_time_ms,
                  bool low_power_xosc,
                  bool low_power_rosc,
                  bool low_power_plls,
                  bool low_power_usb_phy,
                  bool low_power_wifi_chip,
                  uint64_t exclude_gpios_mask);

/*
 * Force dormant mode, waking after sleeping_ms via the POWMAN alarm.
 * sleeping_ms must be > 0.
 */
void powman_off_for_ms(uint32_t sleeping_ms,
                        powman_before_sleep_t before_sleep,
                        uint64_t exclude_gpios_mask);

/*
 * Force dormant mode, waking when `gpio` reaches level `high`.
 * slot selects which of the 4 PWRUP registers to use (POWMAN_PWRUP0 is a
 * reasonable default when only one GPIO wake source is needed).
 */
void powman_off_until_gpio(uint8_t gpio,
                            bool high,
                            uint32_t slot,
                            powman_before_sleep_t before_sleep,
                            uint64_t exclude_gpios_mask);

/*
 * Force dormant mode, waking when ANY of up to 4 GPIOs reaches its target
 * level. pins/num_pins: 1-4 wake-up conditions, one per PWRUP slot (in
 * order). Use powman_get_wakeup_reason() after reboot to tell which one
 * fired (POWMAN_WAKEUP_GPIO0..GPIO3 correspond to pins[0]..pins[3]).
 */
void powman_off_until_any_gpio(const powman_gpio_wake_t *pins,
                                size_t num_pins,
                                powman_before_sleep_t before_sleep,
                                uint64_t exclude_gpios_mask);

/*
 * Force dormant mode, waking on EITHER the timer alarm expiring OR any of
 * up to 4 GPIOs reaching its target level — whichever happens first.
 * sleeping_ms must be > 0. pins/num_pins: 0-4 wake-up conditions.
 * Use powman_get_wakeup_reason() after reboot to tell which one fired
 * (POWMAN_WAKEUP_ALARM for the timer, POWMAN_WAKEUP_GPIO0..GPIO3 for
 * pins[0]..pins[3]).
 */
void powman_off_for_ms_or_gpio(uint32_t sleeping_ms,
                                const powman_gpio_wake_t *pins,
                                size_t num_pins,
                                powman_before_sleep_t before_sleep,
                                uint64_t exclude_gpios_mask);

/*
 * Returns the POWMAN_WAKEUP_* bitmask describing why the chip rebooted, or
 * 0 on a fresh power-on / plain software reset (i.e. POWMAN never powered
 * SWCORE down, so there's nothing to report).
 */
uint32_t powman_get_wakeup_reason(void);

/* ------------------------------------------------------------------------
 * EXPERIMENTAL / optional low-power steps. Call these directly (e.g. from
 * a before_sleep callback) for one-off use, or enable them via powman_init
 * so every powman_off*() call applies them automatically.
 * ------------------------------------------------------------------------ */

/*
 * Move clk_ref/clk_sys off XOSC onto the ring oscillator (ROSC), then stop
 * XOSC. Only call this right before going to sleep.
 *
 * RISK: this only works from the stock boot clock configuration. If
 * clk_sys is not sourced from the XOSC/PLL path when this runs, or if
 * something downstream still depends on XOSC after this call, the system
 * clock can disappear and the chip hangs (requires a physical
 * reset/reflash to recover).
 */
void powman_stop_xosc(void);

/*
 * Move clk_ref (and, transitively, clk_sys) off ROSC onto POWMAN's own
 * always-on low-power oscillator (LPOSC), then stop ROSC.
 * Must be called AFTER powman_stop_xosc() — it assumes clk_ref is
 * currently sourced from ROSC.
 */
void powman_stop_rosc(void);

/*
 * Power down both PLLs (PLL_SYS, PLL_USB).
 * RISK: must be called AFTER powman_stop_xosc() (or with low_power_xosc
 * true on powman_init) — if clk_sys is still sourced from PLL_SYS when
 * this runs, the system clock disappears and the chip hangs.
 */
void powman_power_down_plls(void);

/*
 * Re-isolate the USB PHY before sleeping (MAIN_CTRL.PHY_ISO=1 + DP/DM
 * pulldowns), matching the RP2350 datasheet's own documented low-power
 * methodology. Lower risk than stop_xosc/stop_rosc: doesn't touch
 * clk_sys/clk_ref.
 */
void powman_isolate_usb_phy(void);

/* Power down the CYW43439 wireless chip (Pico 2 W only) by driving its
 * WL_REG_ON pin (POWMAN_CYW43_WL_REG_ON) low. */
void powman_power_down_wifi_chip(void);

#ifdef __cplusplus
}
#endif

#endif /* POWMAN_SLEEP_H */
