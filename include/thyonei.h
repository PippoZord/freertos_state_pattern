#ifndef THYONEI_H
#define THYONEI_H
    #include "uart.h"

    /**
     * @brief Max payload the Thyone-I accepts in a single radio packet
     * (see the user manual, e.g. CMD_BROADCAST_DATA_REQ: "A payload
     * length of maximum 224 bytes can be transmitted per packet"). Any
     * send function with a longer payload must split it across several
     * packets - see ThyoneI_SendFragmented().
     */
    #define THYONEI_MAX_PAYLOAD 224

    typedef struct {
        Uart base;
    } ThyoneI;


    ThyoneI *NewThyoneI(char *name, int timeout, uint32_t uxStackDepth, UBaseType_t uxPriority, uart_inst_t *uart, uint tx, uint rx, uint baudrate, uint expectedLen);

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
     * @brief One packet's worth of a send function: frames and writes out
     * exactly one packet, whose chunk is guaranteed by the caller
     * (ThyoneI_SendFragmented()) to be at most the max_chunk it was given.
     * ctx is whatever the fragmenting caller passed through unchanged -
     * fixed per-message data a framer needs alongside the chunk itself
     * (e.g. a destination address), not itself subject to fragmentation.
     */
    typedef void (*ThyoneISendFn)(ThyoneI *t, void *ctx, uint8_t *chunk, uint16_t chunk_len);

    /**
     * @brief Splits data into chunks of at most max_chunk bytes and calls
     * sendOne once per chunk, in order - the generic building block
     * behind every ThyoneI send function that needs to support payloads
     * longer than a single radio packet can carry. data is never copied;
     * each call gets a pointer straight into the caller's buffer plus the
     * chunk's length.
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
     * @param sendOne Function that frames and sends exactly one packet.
     */
    void ThyoneI_SendFragmented(ThyoneI *t, uint8_t *data, uint16_t len, uint16_t max_chunk, void *ctx, ThyoneISendFn sendOne);

    void ThyoneIBroadcast(ThyoneI *t, uint8_t *msg, uint16_t payload_len);

    void ThyoneISendToAddress(ThyoneI *t, uint8_t *address, uint8_t *msg, uint16_t payload_len);

    void ThyoneIRead(ThyoneI *t, uint8_t *out, uint len);
#endif