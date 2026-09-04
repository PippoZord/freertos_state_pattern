/**
 * @file uart.c
 * @brief Implementation of Uart. For now both TX and RX are synchronous
 * and poll-based, driven entirely from the calling task - no interrupt
 * involved yet (see the @warning on UartRead() in uart.h; interrupt-driven
 * RX buffering is coming back later). UartRead() simply drains whatever
 * the hardware FIFO already has, up to the requested length.
 */

#include "uart.h"
#include <stdlib.h>

/**
 * @brief Uart keeps no periodic work of its own - both TX and RX happen
 * synchronously when UartWrite()/UartRead() are called. Exists only
 * because Agent_Init() requires a non-NULL behave().
 *
 * @param self The agent itself (unused).
 */
static void Uart_Behave(Agent *self) {
    (void)self;
}

/**
 * @brief Releases the TX/RX pins and the UART peripheral before the Uart
 * is freed.
 *
 * @param self The Uart being deleted, as its base Agent.
 */
static void Uart_Delete(Agent *self) {
    Uart *u = (Uart *)self;
    uart_deinit(u->uart);
    gpio_deinit(u->tx);
    gpio_deinit(u->rx);
}

/** @copydoc NewUart */
Uart *NewUart(char *name, int timeout, uint32_t uxStackDepth, UBaseType_t uxPriority, uart_inst_t *uart, uint tx, uint rx, uint baudrate, uint expectedLen) {
    (void)expectedLen; // unused for now - no interrupt-driven message accumulation yet, see uart.h

    Uart *u = malloc(sizeof(Uart));
    u->rx = rx;
    u->tx = tx;
    u->baudrate = baudrate;
    u->uart = uart;
    u->rx_len = 0;
    u->expectedLen = expectedLen;
    u->rx_ready = false;

    uart_init(uart, baudrate);
    gpio_set_function(tx, UART_FUNCSEL_NUM(uart, tx));
    gpio_set_function(rx, UART_FUNCSEL_NUM(uart, rx));
    uart_set_hw_flow(uart, false, false);
    // FIFO on: with no interrupt draining it, this is what keeps a few
    // received bytes safe between one UartRead() call and the next.
    uart_set_fifo_enabled(uart, true);

    Agent_Init(&u->base, name, timeout, uxStackDepth, uxPriority, Uart_Behave, Uart_Delete);
    return u;
}

/** @copydoc UartWrite */
void UartWrite(Uart *u, uint8_t *bytes, size_t len) {
    for (size_t i = 0; i < len; i++) {
        uart_putc(u->uart, bytes[i]);
    }
}

/** @copydoc UartRead */
uint UartRead(Uart *u, uint8_t *out, uint len) {
    uint n = 0;
    while (n < len && uart_is_readable(u->uart)) {
        out[n++] = uart_getc(u->uart);
    }
    return n;
}
