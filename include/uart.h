#ifndef UART_H
#define UART_H

    #include "agent.h"
    #include "pico/stdlib.h"
    #include "hardware/uart.h"

    /** @brief Capacity of Uart's RX ring buffer, in bytes. */
    #define UART_RX_RING_SIZE 256

    /**
     * @brief A UART port, as an Agent extension: base (Agent) plus the
     * pins/baudrate it was configured with. TX is synchronous
     * (UartWrite() blocks on uart_putc()). RX is interrupt-driven but
     * deliberately dumb: the RX interrupt (see on_uart0_rx()/on_uart1_rx()
     * in uart.c) does nothing but copy whatever bytes the hardware FIFO
     * has into rx_ring - no framing, no protocol awareness, so it stays
     * minimal and safe to run in interrupt context. UartRead() drains
     * rx_ring from task context; all the actual message framing lives
     * there (e.g. see thyonei.c), never in the ISR.
     *
     * rx_head is only ever written by the ISR, rx_tail only ever written
     * by whichever task calls UartRead() - a standard single-producer/
     * single-consumer ring buffer, safe without extra locking as long as
     * only one task ever calls UartRead() on a given Uart.
     */
    typedef struct {
        Agent base;
        uart_inst_t *uart;
        uint tx;
        uint rx;
        uint baudrate;

        uint8_t rx_ring[UART_RX_RING_SIZE];
        volatile uint16_t rx_head;
        volatile uint16_t rx_tail;
    } Uart;

    /**
     * @brief Allocates a new Uart, configures its TX/RX pins and baud
     * rate, and enables its RX interrupt. Both uart0 and uart1 can be
     * used at once, independently - each is tracked in its own slot,
     * looked up by uart_get_index() when its interrupt fires (see
     * uart.c), so this isn't limited to one Uart at a time the way an
     * early version of ToggleButton originally was.
     *
     * @param name Name of the task/agent (copied internally, see Agent_Init).
     * @param timeout Period in ms between behave() calls; use 0 (no task).
     * @param uxStackDepth Task stack depth, in words (moot if timeout == 0).
     * @param uxPriority FreeRTOS task priority (moot if timeout == 0).
     * @param uart Which UART peripheral (uart0 or uart1).
     * @param tx GPIO pin for TX.
     * @param rx GPIO pin for RX.
     * @param baudrate Baud rate, e.g. 115200.
     * @return Uart* Newly heap-allocated Uart, ready for UartWrite()/UartRead().
     */
    Uart *NewUart(char *name, int timeout, uint32_t uxStackDepth, UBaseType_t uxPriority, uart_inst_t *uart, uint tx, uint rx, uint baudrate);

    /**
     * @brief Disables the RX interrupt, releases the TX/RX pins and the
     * UART peripheral. Exposed (not static) so a derived type that has
     * nothing extra of its own to release (e.g. ThyoneI) can reuse it
     * directly as its own AgentDelete, the same way NewUart() does.
     *
     * @param self The Uart being deleted, as its base Agent.
     */
    void Uart_Delete(Agent *self);

    /**
     * @brief Initializes an already-allocated Uart in place: hardware
     * setup (pins, baud rate, RX interrupt) plus Agent_Init() with the
     * given behave/delete - the same pattern used everywhere else
     * (Output_Init(), Pwm_Init(), ...), so a derived type (e.g. ThyoneI)
     * can reuse Uart's hardware setup while supplying its own behave().
     *
     * @param u Uart to initialize (memory already allocated by the caller).
     * @param uart Which UART peripheral (uart0 or uart1).
     * @param tx GPIO pin for TX.
     * @param rx GPIO pin for RX.
     * @param baudrate Baud rate, e.g. 115200.
     * @param behave Behaviour function passed through to Agent_Init().
     * @param delete Delete function passed through to Agent_Init().
     * @param name Name of the task/agent (copied internally, see Agent_Init).
     * @param timeout Period in ms between behave() calls; use 0 (no task).
     * @param uxStackDepth Task stack depth, in words (moot if timeout == 0).
     * @param uxPriority FreeRTOS task priority (moot if timeout == 0).
     */
    void Uart_Init(Uart *u, uart_inst_t *uart, uint tx, uint rx, uint baudrate, AgentBehaviour behave, AgentDelete delete, char *name, int timeout, uint32_t uxStackDepth, UBaseType_t uxPriority);

    /**
     * @brief Writes len bytes out over TX. Blocking (uart_putc() per byte).
     *
     * @param u Uart to write to.
     * @param bytes Bytes to send.
     * @param len How many bytes to send.
     */
    void UartWrite(Uart *u, uint8_t *bytes, size_t len);

    /**
     * @brief Reads up to len bytes already received into rx_ring by the
     * RX interrupt. Non-blocking: returns immediately with however many
     * bytes are actually available right now (0..len) - if fewer than
     * len are ready, it returns just those, it does not wait for the
     * rest. The caller is expected to call again later for the
     * remainder (see ThyoneI_WaitForBytes() in thyonei.h for a helper
     * that does exactly that).
     *
     * @param u Uart to read from.
     * @param out Destination buffer.
     * @param len How many bytes the caller would like to read.
     * @return uint Number of bytes actually copied (0..len).
     */
    uint UartRead(Uart *u, uint8_t *out, uint len);

#endif
