#ifndef UART_H
#define UART_H

    #include "agent.h"
    #include "pico/stdlib.h"
    #include "hardware/uart.h"

    #define MAX_RX 256

    /**
     * @brief A UART port, as an Agent extension: base (Agent) plus the
     * pins/baudrate it was configured with. For now both TX and RX are
     * synchronous/poll-based (UartWrite()/UartRead() in uart.c) - no
     * interrupt involved. rx_buff/rx_len/expectedLen/rx_ready are not
     * used yet; they're reserved for when interrupt-driven RX buffering
     * comes back.
     */
    typedef struct {
        Agent base;
        uart_inst_t *uart;
        uint tx;
        uint rx;
        uint baudrate;

        uint8_t rx_buff[MAX_RX];
        volatile uint rx_len;
        uint expectedLen;
        volatile bool rx_ready;
    } Uart;

    /**
     * @brief Allocates a new Uart and configures its TX/RX pins and baud
     * rate. Both uart0 and uart1 can be used at once, independently -
     * each is its own instance with its own struct, so this isn't
     * limited to one Uart at a time the way an early version of
     * ToggleButton originally was.
     *
     * @param name Name of the task/agent (copied internally, see Agent_Init).
     * @param timeout Period in ms between behave() calls; use 0 (no task).
     * @param uxStackDepth Task stack depth, in words (moot if timeout == 0).
     * @param uxPriority FreeRTOS task priority (moot if timeout == 0).
     * @param uart Which UART peripheral (uart0 or uart1).
     * @param tx GPIO pin for TX.
     * @param rx GPIO pin for RX.
     * @param baudrate Baud rate, e.g. 115200.
     * @param expectedLen Not used yet (reserved for when interrupt-driven
     * message accumulation comes back); pass anything, it's ignored.
     * @return Uart* Newly heap-allocated Uart, ready for UartWrite()/UartRead().
     */
    Uart *NewUart(char *name, int timeout, uint32_t uxStackDepth, UBaseType_t uxPriority, uart_inst_t *uart, uint tx, uint rx, uint baudrate, uint expectedLen);

    /**
     * @brief Writes len bytes out over TX. Blocking (uart_putc() per byte).
     *
     * @param u Uart to write to.
     * @param bytes Bytes to send.
     * @param len How many bytes to send.
     */
    void UartWrite(Uart *u, uint8_t *bytes, size_t len);

    /**
     * @brief Reads up to len bytes already received. Non-blocking: returns
     * immediately with however many bytes are actually available right
     * now (0..len) - if fewer than len are ready, it returns just those,
     * it does not wait for the rest. The caller is expected to call again
     * later for the remainder.
     *
     * @param u Uart to read from.
     * @param out Destination buffer.
     * @param len How many bytes the caller would like to read.
     * @return uint Number of bytes actually copied (0..len).
     */
    uint UartRead(Uart *u, uint8_t *out, uint len);

#endif
