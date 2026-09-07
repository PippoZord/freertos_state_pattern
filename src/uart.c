/**
 * @file uart.c
 * @brief Implementation of Uart. TX is synchronous. RX is
 * interrupt-driven but deliberately dumb: the ISR only copies bytes
 * from the hardware FIFO into rx_ring, nothing else - no framing, no
 * printf, no protocol awareness, same "keep the ISR minimal" approach
 * used for every other interrupt in this project (see togglebutton.c).
 * All the real work (message framing) happens in task context, reading
 * from rx_ring via UartRead() - see thyonei.c.
 */

#include "uart.h"
#include <stdlib.h>

/**
 * @brief The Uart currently registered for each hardware UART's RX
 * interrupt, indexed by uart_get_index() (0 = uart0, 1 = uart1). Both
 * can be in use at once, independently - each has its own dedicated
 * NVIC line, so there is no single-callback bottleneck to work around
 * here the way there is for GPIO interrupts (see peripheral.h).
 */
static Uart *instances[2];

/**
 * @brief Uart keeps no periodic work of its own - TX happens
 * synchronously when UartWrite() is called, RX is drained by the
 * interrupt into rx_ring and read out via UartRead(). Exists only
 * because Agent_Init() requires a non-NULL behave().
 *
 * @param self The agent itself (unused).
 */
static void Uart_Behave(Agent *self) {
    (void)self;
}

/**
 * @brief Disables the RX interrupt, releases the TX/RX pins and the
 * UART peripheral before the Uart is freed, and clears its slot in
 * instances[] so the interrupt handler can no longer reach it.
 *
 * @param self The Uart being deleted, as its base Agent.
 */
void Uart_Delete(Agent *self) {
    Uart *u = (Uart *)self;
    uint idx = uart_get_index(u->uart);
    int UART_IRQ = idx == 0 ? UART0_IRQ : UART1_IRQ;
    uart_set_irq_enables(u->uart, false, false);
    irq_set_enabled(UART_IRQ, false);
    if (instances[idx] == u) {
        instances[idx] = NULL;
    }
    uart_deinit(u->uart);
    gpio_deinit(u->tx);
    gpio_deinit(u->rx);
}

/**
 * @brief Pushes one byte into u's RX ring buffer. If the ring is full
 * (the consuming task hasn't kept up), the byte is silently dropped -
 * there is nowhere else to put it in interrupt context.
 */
static void Uart_RxRingPush(Uart *u, uint8_t byte) {
    uint16_t next = (uint16_t)((u->rx_head + 1) % UART_RX_RING_SIZE);
    if (next == u->rx_tail) {
        return; // ring full, drop the byte
    }
    u->rx_ring[u->rx_head] = byte;
    u->rx_head = next;
}

/**
 * @brief Real interrupt-context handler for u's RX: drains whatever the
 * hardware FIFO currently holds into rx_ring, byte by byte, nothing
 * else - deliberately as minimal as every other ISR in this project.
 */
static void Uart_DrainRxIrq(Uart *u) {
    while (uart_is_readable(u->uart)) {
        Uart_RxRingPush(u, uart_getc(u->uart));
    }
}

/** @brief Real interrupt-context handler for uart0's RX, registered with irq_set_exclusive_handler(). */
static void on_uart0_rx(void) {
    if (instances[0] != NULL) {
        Uart_DrainRxIrq(instances[0]);
    }
}

/** @brief Real interrupt-context handler for uart1's RX, registered with irq_set_exclusive_handler(). */
static void on_uart1_rx(void) {
    if (instances[1] != NULL) {
        Uart_DrainRxIrq(instances[1]);
    }
}

/** @copydoc Uart_Init */
void Uart_Init(Uart *u, uart_inst_t *uart, uint tx, uint rx, uint baudrate, AgentBehaviour behave, AgentDelete delete, char *name, int timeout, uint32_t uxStackDepth, UBaseType_t uxPriority) {
    u->rx = rx;
    u->tx = tx;
    u->baudrate = baudrate;
    u->uart = uart;
    u->rx_head = 0;
    u->rx_tail = 0;

    uart_init(uart, baudrate);
    gpio_set_function(tx, UART_FUNCSEL_NUM(uart, tx));
    gpio_set_function(rx, UART_FUNCSEL_NUM(uart, rx));
    uart_set_hw_flow(uart, false, false);
    uart_set_fifo_enabled(uart, true);

    uint idx = uart_get_index(uart);
    instances[idx] = u;
    int UART_IRQ = idx == 0 ? UART0_IRQ : UART1_IRQ;
    irq_set_exclusive_handler(UART_IRQ, idx == 0 ? on_uart0_rx : on_uart1_rx);
    irq_set_enabled(UART_IRQ, true);
    uart_set_irq_enables(uart, true, false); // RX (+ RX timeout) interrupt on, TX interrupt off

    Agent_Init(&u->base, name, timeout, uxStackDepth, uxPriority, behave, delete);
}

/** @copydoc NewUart */
Uart *NewUart(char *name, int timeout, uint32_t uxStackDepth, UBaseType_t uxPriority, uart_inst_t *uart, uint tx, uint rx, uint baudrate) {
    Uart *u = malloc(sizeof(Uart));
    Uart_Init(u, uart, tx, rx, baudrate, Uart_Behave, Uart_Delete, name, timeout, uxStackDepth, uxPriority);
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
    while (n < len && u->rx_tail != u->rx_head) {
        out[n++] = u->rx_ring[u->rx_tail];
        u->rx_tail = (uint16_t)((u->rx_tail + 1) % UART_RX_RING_SIZE);
    }
    return n;
}
