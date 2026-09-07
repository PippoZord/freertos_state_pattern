#ifndef THYONEI_H
#define THYONEI_H
    #include "uart.h"
    #include <stdbool.h>

    /**
     * @brief Max payload the Thyone-I accepts in a single radio packet
     * (see the user manual, e.g. CMD_BROADCAST_DATA_REQ: "A payload
     * length of maximum 224 bytes can be transmitted per packet"). Any
     * send function with a longer payload must split it across several
     * packets - see ThyoneI_SendFragmented().
     */
    #define THYONEI_MAX_PAYLOAD 224

    /** @brief How long ThyoneI_WaitForBytes() sleeps between UartRead() retries. */
    #define THYONEI_WAIT_POLL_MS 5

    /** @brief How many messages from other devices ThyoneIReceive() can hold at once before older ones are dropped. */
    #define THYONEI_INBOX_DEPTH 4

    /**
     * @brief One CMD_DATA_IND, decoded: who sent it, how strong the
     * signal was, and the payload itself. Filled by ThyoneI's own
     * background task as messages arrive, handed out (oldest first) by
     * ThyoneIReceive().
     */
    typedef struct {
        uint8_t src_address[4];
        int8_t rssi;
        uint8_t payload[THYONEI_MAX_PAYLOAD];
        uint16_t payload_len;
    } ThyoneIMessage;

    /**
     * @brief A Thyone-I radio module, as a Uart extension: base (Uart)
     * plus everything needed to sort incoming bytes into "confirmation
     * of something we just sent" vs "a message from another device".
     *
     * Nothing here is touched directly by UartRead()/the RX interrupt -
     * they only ever fill Uart's own rx_ring (raw bytes, no framing).
     * ThyoneI's own periodic task (see ThyoneI_Behave() in thyonei.c) is
     * the only thing that ever parses rx_ring into complete frames and
     * routes them here: CMD_DATA_CNF/CMD_TXCOMPLETE_RSP update
     * cnf_ready/tx_complete_ready (consumed by whichever task is
     * currently sending, see ThyoneI_WaitForTxComplete()); anything else
     * (CMD_DATA_IND) goes into inbox (consumed by ThyoneIReceive()).
     *
     * @warning Only one send at a time is supported - cnf_ready/
     * tx_complete_ready are a single shared slot, not one per in-flight
     * packet. Fine for this project (state.c sends from a single task,
     * one packet at a time); a second concurrent sender would need its
     * own slot.
     */
    typedef struct {
        Uart base;

        ThyoneIMessage inbox[THYONEI_INBOX_DEPTH];
        uint8_t inbox_head;
        uint8_t inbox_count;

        volatile bool cnf_ready;
        volatile uint8_t cnf_status;
        volatile bool tx_complete_ready;
        volatile uint8_t tx_complete_status;
    } ThyoneI;


    /**
     * @brief Allocates a new ThyoneI, reusing Uart_Init() for the
     * hardware setup (pins, baud rate, RX interrupt) but with its own
     * behave() - a periodic background task that parses whatever raw
     * bytes the RX interrupt has collected into complete frames and
     * routes them (see the @brief on ThyoneI). This is what lets
     * messages from other devices be received even while nothing is
     * being sent.
     *
     * @param name Name of the task/agent (copied internally, see Agent_Init).
     * @param timeout Period in ms between background-task ticks; keep
     * this short (a handful of ms) since it's also how quickly a sent
     * packet's confirmation gets noticed and messages get moved into
     * the inbox.
     * @param uxStackDepth Task stack depth, in words.
     * @param uxPriority FreeRTOS task priority.
     * @param uart Which UART peripheral (uart0 or uart1).
     * @param tx GPIO pin for TX.
     * @param rx GPIO pin for RX.
     * @param baudrate Baud rate, e.g. 115200.
     * @return ThyoneI* Newly heap-allocated, already-listening ThyoneI.
     */
    ThyoneI *NewThyoneI(char *name, int timeout, uint32_t uxStackDepth, UBaseType_t uxPriority, uart_inst_t *uart, uint tx, uint rx, uint baudrate);

    /**
     * @brief XOR checksum, computed incrementally so it can be run over
     * several separate chunks (e.g. a header and a payload) without ever
     * assembling them into one contiguous buffer first.
     *
     * @param acc Checksum so far; pass 0 to start a new checksum.
     * @param bytes Next chunk of bytes to fold in.
     * @param len How many bytes in this chunk.
     * @return uint8_t Updated checksum, to pass as acc on the next chunk
     * (or to use as-is once every chunk has been folded in).
     */
    uint8_t ThyoneIChecksum(uint8_t acc, uint8_t *bytes, uint16_t len);

    /**
     * @brief Blocks the calling task until len bytes have been read from
     * u, or timeoutMs has elapsed without reaching len - polls
     * UartRead() and sleeps THYONEI_WAIT_POLL_MS between attempts
     * (rather than busy-waiting), so this is meant for a task context,
     * not an ISR. Generic: knows nothing about the Thyone-I command
     * format, just "wait for exactly this many bytes". Pass
     * timeoutMs == 0 for a single non-blocking attempt.
     *
     * @param t ThyoneI to read from.
     * @param out Destination buffer, at least len bytes.
     * @param len How many bytes to wait for.
     * @param timeoutMs Give up after this many ms without reaching len.
     * @return bool True once len bytes have been collected into out;
     * false on timeout (out holds whatever partial bytes arrived).
     */
    bool ThyoneI_WaitForBytes(ThyoneI *t, uint8_t *out, uint len, uint32_t timeoutMs);

    /**
     * @brief One packet's worth of a send function: frames, writes out
     * and confirms exactly one packet, whose chunk is guaranteed by the
     * caller (ThyoneI_SendFragmented()) to be at most the max_chunk it
     * was given. ctx is whatever the fragmenting caller passed through
     * unchanged - fixed per-message data a framer needs alongside the
     * chunk itself (e.g. a destination address), not itself subject to
     * fragmentation.
     *
     * @return bool Whether this one packet was sent and confirmed
     * successfully.
     */
    typedef bool (*ThyoneISendFn)(ThyoneI *t, void *ctx, uint8_t *chunk, uint16_t chunk_len);

    /**
     * @brief Splits data into chunks of at most max_chunk bytes and calls
     * sendOne once per chunk, in order, stopping at the first chunk that
     * fails - the generic building block behind every ThyoneI send
     * function that needs to support payloads longer than a single radio
     * packet can carry. data is never copied; each call gets a pointer
     * straight into the caller's buffer plus the chunk's length.
     *
     * @param t ThyoneI to send on.
     * @param data Full payload to send, of any length.
     * @param len Length of data, in bytes.
     * @param max_chunk Largest chunk sendOne can take in one call - e.g.
     * THYONEI_MAX_PAYLOAD for a framer with no other overhead, or less if
     * sendOne also packs fixed extra data (such as an address) into the
     * same THYONEI_MAX_PAYLOAD-bounded packet.
     * @param ctx Passed through unchanged to every sendOne call; NULL if
     * sendOne needs nothing beyond the chunk itself.
     * @param sendOne Function that frames, sends and confirms exactly
     * one packet.
     * @return bool True once every chunk has been sent and confirmed;
     * false as soon as one chunk's sendOne() call fails (remaining
     * chunks are not attempted).
     */
    bool ThyoneI_SendFragmented(ThyoneI *t, uint8_t *data, uint16_t len, uint16_t max_chunk, void *ctx, ThyoneISendFn sendOne);

    /**
     * @brief Broadcasts msg, splitting it into as many packets as needed
     * (see ThyoneI_SendFragmented()). Each packet's CMD_DATA_CNF and
     * CMD_TXCOMPLETE_RSP confirmation is waited for and checked before
     * the next packet is sent - callers don't need to (and shouldn't)
     * separately read those bytes back themselves.
     *
     * @return bool True if every packet was sent and confirmed; false
     * otherwise (a bad status byte, or no confirmation within the wait
     * timeout).
     */
    bool ThyoneIBroadcast(ThyoneI *t, uint8_t *msg, uint16_t payload_len);

    /** @copydoc ThyoneIBroadcast, sent to address instead of broadcast. */
    bool ThyoneISendToAddress(ThyoneI *t, uint8_t *address, uint8_t *msg, uint16_t payload_len);

    /**
     * @brief Hands out the oldest message received from another device
     * since the last call, if any - non-blocking. Messages arrive into
     * the inbox on their own, filled by ThyoneI's background task, so
     * this can be called at any time regardless of whether anything is
     * currently being sent.
     *
     * @param t ThyoneI to receive from.
     * @param out Filled with the oldest pending message, if any.
     * @return bool True if a message was available and out was filled;
     * false if the inbox was empty.
     */
    bool ThyoneIReceive(ThyoneI *t, ThyoneIMessage *out);
#endif
