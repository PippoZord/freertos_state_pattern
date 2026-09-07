/*
 * powman_sleep.c — see powman_sleep.h.
 *
 * Direct port of the MicroPython `powman` module: same register addresses,
 * same offsets, same sequencing — just mem32[addr] = val turned into
 * REG32(addr) = val.
 */

#include "powman_sleep.h"

#include "hardware/gpio.h"
#include "hardware/regs/addressmap.h" /* POWMAN_BASE, CLOCKS_BASE, XOSC_BASE, ROSC_BASE,
                                        * PADS_BANK0_BASE, PLL_SYS_BASE, PLL_USB_BASE,
                                        * USBCTRL_REGS_BASE */
#include "hardware/sync.h"            /* __wfi() */
#include "pico/assert.h"

/* Raw 32-bit register access, equivalent to MicroPython's machine.mem32. */
#define REG32(addr) (*(volatile uint32_t *)(uintptr_t)(addr))

/* ---- POWMAN password ----------------------------------------------------- */
#define PASS 0x5AFE0000u

/* ---- POWMAN register offsets ------------------------------------------- */
#define VREG_LP_ENTRY 0x10u
#define STATE         0x38u
#define TIMER         0x88u
#define INTE          0xE4u

#define SET_TIME_15TO0  0x6Cu
#define SET_TIME_31TO16 0x68u
#define SET_TIME_47TO32 0x64u
#define SET_TIME_63TO48 0x60u

#define ALARM_TIME_15TO0  0x84u
#define ALARM_TIME_31TO16 0x80u
#define ALARM_TIME_47TO32 0x7Cu
#define ALARM_TIME_63TO48 0x78u

#define READ_TIME_UPPER 0x70u
#define READ_TIME_LOWER 0x74u

#define BOOT0 0xD0u
#define BOOT1 0xD4u
#define BOOT2 0xD8u
#define BOOT3 0xDCu

#define DBG_PWRCFG 0xA4u

#define CHIP_RESET        0x2Cu
#define LAST_SWCORE_PWRUP 0xA0u

static const uint32_t POWMAN_PWRUP_REGS[4] = {
    POWMAN_PWRUP0, POWMAN_PWRUP1, POWMAN_PWRUP2, POWMAN_PWRUP3};

/* ---- module state (mirrors the Python module's _LOW_POWER dict) -------- */
typedef struct {
    bool rosc;
    bool plls;
    bool usb_phy;
    bool wifi_chip;
    uint64_t exclude_gpios_mask;
} powman_low_power_cfg_t;

static powman_low_power_cfg_t s_low_power = {0};

void powman_init(uint64_t abs_time_ms,
                  bool low_power_xosc,
                  bool low_power_rosc,
                  bool low_power_plls,
                  bool low_power_usb_phy,
                  bool low_power_wifi_chip,
                  uint64_t exclude_gpios_mask) {
    hard_assert(abs_time_ms >= 1);

    /* Stop timer */
    REG32(POWMAN_BASE + TIMER) = PASS | 0x00u;

    REG32(POWMAN_BASE + POWMAN_PWRUP0) = PASS | 0x200u;

    /* Set time (64 bit split in 4 x 16 bit) */
    REG32(POWMAN_BASE + SET_TIME_15TO0)  = PASS | (uint32_t)(abs_time_ms & 0xFFFFu);
    REG32(POWMAN_BASE + SET_TIME_31TO16) = PASS | (uint32_t)((abs_time_ms >> 16) & 0xFFFFu);
    REG32(POWMAN_BASE + SET_TIME_47TO32) = PASS | (uint32_t)((abs_time_ms >> 32) & 0xFFFFu);
    REG32(POWMAN_BASE + SET_TIME_63TO48) = PASS | (uint32_t)((abs_time_ms >> 48) & 0xFFFFu);

    /* Start timer: RUN + CLEAR + CLEAR ALARM */
    REG32(POWMAN_BASE + TIMER) = PASS | 0x46u;

    /* Ignore debugger */
    REG32(POWMAN_BASE + DBG_PWRCFG) = PASS | 0x01u;

    if (low_power_xosc) {
        powman_stop_xosc();
    }

    s_low_power.rosc = low_power_rosc;
    s_low_power.plls = low_power_plls;
    s_low_power.usb_phy = low_power_usb_phy;
    s_low_power.wifi_chip = low_power_wifi_chip;
    s_low_power.exclude_gpios_mask = exclude_gpios_mask;
}

/* Return current POWMAN time (64-bit). */
static uint64_t powman_get_current_time(void) {
    uint32_t hi1, lo, hi2;
    for (;;) {
        hi1 = REG32(POWMAN_BASE + READ_TIME_UPPER);
        lo  = REG32(POWMAN_BASE + READ_TIME_LOWER);
        hi2 = REG32(POWMAN_BASE + READ_TIME_UPPER);
        if (hi1 == hi2) {
            return ((uint64_t)hi1 << 32) | lo;
        }
    }
}

static void powman_force_reboot(void) {
    REG32(POWMAN_BASE + BOOT0) = 0;
    REG32(POWMAN_BASE + BOOT1) = 0;
    REG32(POWMAN_BASE + BOOT2) = 0;
    REG32(POWMAN_BASE + BOOT3) = 0;
}

/* Applies whichever optional low-power steps were enabled via powman_init().
 * Called last, right before __wfi() — everything else (arming alarm/GPIOs)
 * must already be done, since code after this may run much slower. */
static void powman_apply_low_power_config(void) {
    if (s_low_power.rosc) {
        powman_stop_rosc();
    }
    if (s_low_power.plls) {
        powman_power_down_plls();
    }
    if (s_low_power.usb_phy) {
        powman_isolate_usb_phy();
    }
    if (s_low_power.wifi_chip) {
        powman_power_down_wifi_chip();
    }
}

/* Default range of GPIOs automatically forced low before sleep, skipping
 * whichever ones are excluded (wake GPIOs, plus anything the caller lists
 * via exclude_gpios_mask). GP23-29 are left out of this range on purpose:
 * on Pico 2 W several of them are reserved for the wireless chip
 * (23, 24, 25, 29) or carry the ADC reverse-diode caveat (26-28) — forcing
 * those low unconditionally could interfere with functions this library
 * doesn't control. Only GP0-22 (plain digital GPIO) are touched
 * automatically. */
#define QUIESCE_GPIO_FIRST 0
#define QUIESCE_GPIO_LAST  22 /* inclusive */

/* Forces every GPIO in [QUIESCE_GPIO_FIRST, QUIESCE_GPIO_LAST] that isn't in
 * exclude_mask to LOW before sleeping — e.g. to cut power to an accessory
 * wired to a GPIO instead of 3V3 (which can't be switched off from
 * software). GPIO pads live in the always-on domain (same reason the wake
 * GPIOs keep working with SWCORE off), so they stay live throughout
 * dormant regardless of what else is powered down.
 *
 * This actively drives each pin low, then also sets the pad's own
 * pull-down as a fallback for once SWCORE (and the SIO logic actually
 * doing the driving) powers off, and disables the pad's input buffer (IE)
 * since nothing reads these pins while quiesced — RP2350 datasheet
 * §14.9.4 (IO electrical characteristics): "The input buffer can be
 * disabled, to reduce current consumption when the pad is unused,
 * unconnected or driven statically." If any excluded GPIO is left
 * floating on purpose, or any non-excluded GPIO is wired to something
 * else actively driving it from outside, forcing it low here will fight
 * that external driver — only exclude/wire accordingly. */
static void powman_quiesce_unused_gpios(uint64_t exclude_mask) {
    for (int gpio = QUIESCE_GPIO_FIRST; gpio <= QUIESCE_GPIO_LAST; gpio++) {
        if (exclude_mask & (1ull << gpio)) {
            continue;
        }
        gpio_init(gpio);
        gpio_set_dir(gpio, GPIO_OUT);
        gpio_put(gpio, 0);
        uint32_t pad_ctrl = PADS_BANK0_BASE + (uint32_t)((gpio + 1) * 4);
        uint32_t val = REG32(pad_ctrl);
        val &= ~(1u << 6); /* IE = 0: input buffer off, unused while quiesced */
        val |= (1u << 2);  /* PDE = 1: fallback pull-down once SWCORE stops driving the pin */
        REG32(pad_ctrl) = val;
    }
}

/* Force dormant mode and set reboot enable.
 * exclude_gpios_mask lists whichever GPIOs must NOT be forced low by the
 * automatic GPIO quiescing above — wake GPIOs (added automatically by the
 * caller) plus anything the library user passed via exclude_gpios_mask on
 * a powman_off*() call. before_sleep, if given, is called last (after
 * powman_apply_low_power_config() and right before __wfi()) for any
 * additional custom setup callers want to run. */
static void powman_power_off(powman_before_sleep_t before_sleep, uint64_t exclude_gpios_mask) {
    /* Set low power mode */
    REG32(POWMAN_BASE + VREG_LP_ENTRY) = PASS | 0x0004u;

    powman_force_reboot();

    /* Switch off system: bit 3 SWCORE, bit 2 XIP, bit 1 SRAM0, bit 0 SRAM1 */
    REG32(POWMAN_BASE + STATE) = PASS | 0x00F0u;

    powman_quiesce_unused_gpios(exclude_gpios_mask | s_low_power.exclude_gpios_mask);

    powman_apply_low_power_config();

    if (before_sleep) {
        before_sleep();
    }

    /* Wait for interrupt / alarm */
    __wfi();
}

/* Arm the alarm timer to fire after sleeping_ms. sleeping_ms must be > 0. */
static void powman_arm_alarm(uint32_t sleeping_ms) {
    hard_assert(sleeping_ms >= 1);

    uint64_t alarm_time = (uint64_t)sleeping_ms + powman_get_current_time();

    /* Stop timer */
    REG32(POWMAN_BASE + TIMER) = PASS | 0x00u;

    /* Write alarm time */
    REG32(POWMAN_BASE + ALARM_TIME_15TO0)  = PASS | (uint32_t)(alarm_time & 0xFFFFu);
    REG32(POWMAN_BASE + ALARM_TIME_31TO16) = PASS | (uint32_t)((alarm_time >> 16) & 0xFFFFu);
    REG32(POWMAN_BASE + ALARM_TIME_47TO32) = PASS | (uint32_t)((alarm_time >> 32) & 0xFFFFu);
    REG32(POWMAN_BASE + ALARM_TIME_63TO48) = PASS | (uint32_t)((alarm_time >> 48) & 0xFFFFu);

    /* Start timer + reset alarm bit */
    REG32(POWMAN_BASE + TIMER) = PASS | 0x72u;
}

void powman_off_for_ms(uint32_t sleeping_ms, powman_before_sleep_t before_sleep, uint64_t exclude_gpios_mask) {
    /* Enable interrupt */
    REG32(POWMAN_BASE + INTE) = PASS | 0x02u;

    powman_arm_alarm(sleeping_ms);

    powman_power_off(before_sleep, exclude_gpios_mask);
}

uint32_t powman_get_wakeup_reason(void) {
    /* HAD_SWCORE_PD (bit 25) is set only when POWMAN explicitly powered
     * down SWCORE. */
    if (REG32(POWMAN_BASE + CHIP_RESET) & (1u << 25)) {
        return REG32(POWMAN_BASE + LAST_SWCORE_PWRUP);
    }
    return 0; /* fresh power-on or software reset */
}

/* Arm one PWRUP slot to trigger a wake-up on a GPIO transition.
 * IMPORTANT: the GPIO must already be at the OPPOSITE level before calling
 * this. POWMAN uses level-triggered detection and requires a transition to
 * fire. */
static void powman_arm_gpio_wakeup(uint8_t gpio, bool high, uint32_t slot) {
    hard_assert(gpio <= 49);

    uint32_t gpio_pad_ctrl = PADS_BANK0_BASE + (uint32_t)((gpio + 1) * 4);
    /* IE (bit6) always on; PUE (bit3) for active-low, PDE (bit2) for active-high */
    REG32(gpio_pad_ctrl) = high ? 0x44u : 0x48u;

    uint32_t direction = high ? 0x80u : 0x00u; /* bit 7: 1=HIGH_RISING, 0=LOW_FALLING */
    REG32(POWMAN_BASE + slot) = PASS | 0x40u | direction | gpio;
}

void powman_off_until_gpio(uint8_t gpio, bool high, uint32_t slot,
                            powman_before_sleep_t before_sleep, uint64_t exclude_gpios_mask) {
    REG32(POWMAN_BASE + INTE) = 0x02u;

    powman_arm_gpio_wakeup(gpio, high, slot);

    powman_power_off(before_sleep, (1ull << gpio) | exclude_gpios_mask);
}

void powman_off_until_any_gpio(const powman_gpio_wake_t *pins, size_t num_pins,
                                powman_before_sleep_t before_sleep, uint64_t exclude_gpios_mask) {
    hard_assert(num_pins >= 1 && num_pins <= 4);

    REG32(POWMAN_BASE + INTE) = 0x02u;

    uint64_t pins_mask = 0;
    for (size_t i = 0; i < num_pins; i++) {
        powman_arm_gpio_wakeup(pins[i].gpio, pins[i].high, POWMAN_PWRUP_REGS[i]);
        pins_mask |= (1ull << pins[i].gpio);
    }

    powman_power_off(before_sleep, pins_mask | exclude_gpios_mask);
}

void powman_off_for_ms_or_gpio(uint32_t sleeping_ms, const powman_gpio_wake_t *pins, size_t num_pins,
                                powman_before_sleep_t before_sleep, uint64_t exclude_gpios_mask) {
    hard_assert(num_pins <= 4);

    REG32(POWMAN_BASE + INTE) = PASS | 0x02u;

    powman_arm_alarm(sleeping_ms);

    uint64_t pins_mask = 0;
    for (size_t i = 0; i < num_pins; i++) {
        powman_arm_gpio_wakeup(pins[i].gpio, pins[i].high, POWMAN_PWRUP_REGS[i]);
        pins_mask |= (1ull << pins[i].gpio);
    }

    powman_power_off(before_sleep, pins_mask | exclude_gpios_mask);
}

/* =============================================================================
 * EXPERIMENTAL: stopping XOSC/ROSC before sleeping (higher risk)
 *
 * Stops the external crystal (XOSC) and, optionally, the ring oscillator
 * (ROSC) before entering POWMAN dormant mode, to save the current they draw
 * while running (they live in the always-on domain, so the SWCORE/XIP/SRAM
 * power-down above does not touch them). Call powman_stop_xosc() and/or
 * powman_stop_rosc() right before a powman_off*() call — typically via its
 * before_sleep hook, so arming the alarm/GPIOs happens first while the
 * clock is still fast. There is no "wake back up" function: every
 * powman_off*() wakes via a full chip reboot, and the boot ROM
 * reinitializes clocks from scratch, so nothing needs restoring here.
 *
 * The POWMAN alarm timer is unaffected by either of these: per the RP2350
 * register docs, POWMAN_TIMER always starts out clocked from its own
 * internal LPOSC, and only moves to XOSC if something explicitly sets
 * POWMAN_TIMER_USE_XOSC — this library never does, so the alarm keeps
 * ticking correctly regardless of what happens to XOSC/ROSC.
 * ============================================================================= */

#define CLK_REF_CTRL     0x30u
#define CLK_REF_SELECTED 0x38u
#define CLK_SYS_CTRL     0x3Cu
#define CLK_SYS_SELECTED 0x44u

#define XOSC_DORMANT 0x08u
#define ROSC_DORMANT 0x10u

#define OSC_DORMANT_VALUE 0x636F6D61u /* "coma" */

#define CLK_REF_SRC_ROSC    0x0u
#define CLK_REF_SRC_LPOSC   0x3u
#define CLK_SYS_SRC_CLK_REF 0x0u

/* one-hot: bit N set once the glitchless mux has actually settled on source N */
#define CLK_REF_SELECTED_ROSC    (1u << 0)
#define CLK_REF_SELECTED_LPOSC   (1u << 3)
#define CLK_SYS_SELECTED_CLK_REF (1u << 0)

/* Move clk_ref and clk_sys off XOSC/PLL onto the ring oscillator (ROSC),
 * then stop XOSC. Only call this right before going to sleep.
 * ROSC is not touched here: it's enabled by default at power-up, and
 * blindly writing its control register would also clobber its FREQ_RANGE
 * field. */
void powman_stop_xosc(void) {
    /* clk_ref: switch away from XOSC onto ROSC. The glitchless mux doesn't
     * switch instantly, so wait for CLK_REF_SELECTED to confirm it before
     * relying on it downstream (same pattern the pico-sdk's
     * clock_configure() uses). */
    REG32(CLOCKS_BASE + CLK_REF_CTRL) = CLK_REF_SRC_ROSC;
    while (!(REG32(CLOCKS_BASE + CLK_REF_SELECTED) & CLK_REF_SELECTED_ROSC)) {
        tight_loop_contents();
    }

    /* clk_sys: switch away from the PLL/aux path onto clk_ref (now ROSC-backed). */
    REG32(CLOCKS_BASE + CLK_SYS_CTRL) = CLK_SYS_SRC_CLK_REF;
    while (!(REG32(CLOCKS_BASE + CLK_SYS_SELECTED) & CLK_SYS_SELECTED_CLK_REF)) {
        tight_loop_contents();
    }

    /* Nothing left depends on XOSC now — stop it. */
    REG32(XOSC_BASE + XOSC_DORMANT) = OSC_DORMANT_VALUE;
}

/* Move clk_ref (and, transitively, clk_sys) off ROSC onto POWMAN's own
 * low-power oscillator (LPOSC, always on), then stop ROSC.
 * Call this AFTER powman_stop_xosc() — it assumes clk_ref is currently on
 * ROSC. */
void powman_stop_rosc(void) {
    REG32(CLOCKS_BASE + CLK_REF_CTRL) = CLK_REF_SRC_LPOSC;
    while (!(REG32(CLOCKS_BASE + CLK_REF_SELECTED) & CLK_REF_SELECTED_LPOSC)) {
        tight_loop_contents();
    }

    /* Nothing left depends on ROSC now — stop it. */
    REG32(ROSC_BASE + ROSC_DORMANT) = OSC_DORMANT_VALUE;
}

/* =============================================================================
 * PLL power-down (low risk, but must run after clk_sys is off the PLL path)
 *
 * Powers down both PLLs (PLL_SYS, PLL_USB) by writing PLL_PWR back to its
 * own reset value — the same single write the pico-sdk's own pll_deinit()
 * does (PD + DSMPD + POSTDIVPD + VCOPD, all four power-down bits set). Once
 * powman_stop_xosc() has switched clk_sys onto clk_ref (bypassing the
 * PLL/aux path), nothing depends on either PLL anymore, and leaving them
 * running just burns current for nothing.
 * ============================================================================= */

#define PLL_PWR 0x04u

#define PLL_PWR_DOWN_VALUE 0x2Du /* PD | DSMPD | POSTDIVPD | VCOPD */

void powman_power_down_plls(void) {
    REG32(PLL_SYS_BASE + PLL_PWR) = PLL_PWR_DOWN_VALUE;
    REG32(PLL_USB_BASE + PLL_PWR) = PLL_PWR_DOWN_VALUE;
}

/* =============================================================================
 * USB PHY isolation (low risk)
 *
 * Re-isolates the USB PHY before entering POWMAN dormant mode, matching the
 * methodology the RP2350 datasheet itself uses for its own documented
 * low-power current figures (section 14.9.7.2): MAIN_CTRL.PHY_ISO=1 with
 * the DP/DM pulldowns enabled. PHY_ISO defaults to 1 (isolated) at reset
 * and gets cleared by USB init to run the device — this just puts it back
 * before sleeping, since the reboot on wake reinitializes USB from scratch
 * regardless.
 * ============================================================================= */

#define MAIN_CTRL 0x40u
#define SIE_CTRL  0x4Cu

#define MAIN_CTRL_PHY_ISO    (1u << 2)
#define SIE_CTRL_PULLDOWN_EN (1u << 15)

void powman_isolate_usb_phy(void) {
    REG32(USBCTRL_REGS_BASE + MAIN_CTRL) |= MAIN_CTRL_PHY_ISO;
    REG32(USBCTRL_REGS_BASE + SIE_CTRL) |= SIE_CTRL_PULLDOWN_EN;
}

/* CYW43439 wireless chip power-down (Pico 2 W only, low risk). */
void powman_power_down_wifi_chip(void) {
    gpio_init(POWMAN_CYW43_WL_REG_ON);
    gpio_set_dir(POWMAN_CYW43_WL_REG_ON, GPIO_OUT);
    gpio_put(POWMAN_CYW43_WL_REG_ON, 0);
}
