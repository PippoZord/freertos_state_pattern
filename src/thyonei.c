#include "thyonei.h"
#include <stdlib.h>
#include <stdio.h>

ThyoneI *NewThyoneI(char *name, int timeout, uint32_t uxStackDepth, UBaseType_t uxPriority, uart_inst_t *uart, uint tx, uint rx, uint baudrate, uint expectedLen){
    ThyoneI *t = malloc(sizeof(ThyoneI));
    Uart_Init(&t->base, uart, tx, rx,baudrate, expectedLen, name, timeout, uxStackDepth, uxPriority);
    return t;
}

/** @copydoc ThyoneIChecksum */
uint8_t ThyoneIChecksum(uint8_t acc, uint8_t *bytes, uint16_t len){
    for (uint16_t i = 0; i < len; i++) acc ^= bytes[i];
    return acc;
}

/** @copydoc ThyoneI_SendFragmented */
void ThyoneI_SendFragmented(ThyoneI *t, uint8_t *data, uint16_t len, uint16_t max_chunk, void *ctx, ThyoneISendFn sendOne){
    uint16_t offset = 0;
    while (offset < len) {
        uint16_t remaining = len - offset;
        uint16_t chunk_len = remaining > max_chunk ? max_chunk : remaining;
        sendOne(t, ctx, data + offset, chunk_len);
        offset += chunk_len;
    }
}

/**
 * Frames one packet as {0x02, 0x06, len_lo, len_hi, chunk[0..chunk_len), xor_checksum}
 * and writes it out over uart, one piece at a time - the checksum is
 * accumulated on the fly instead of assembling the whole frame in a
 * buffer first. Only ever called with chunk_len <= THYONEI_MAX_PAYLOAD,
 * as guaranteed by ThyoneI_SendFragmented(). ctx is unused - broadcast
 * needs nothing beyond the chunk itself.
 */
static void ThyoneIBroadcast_OnePacket(ThyoneI *t, void *ctx, uint8_t *chunk, uint16_t chunk_len){
    (void)ctx;
    uint8_t header[4] = {
        0x02,
        0x06,
        (uint8_t)(chunk_len & 0xFF),
        (uint8_t)((chunk_len >> 8) & 0xFF),
    };

    uint8_t check = ThyoneIChecksum(0, header, sizeof(header));
    check = ThyoneIChecksum(check, chunk, chunk_len);

    UartWrite(&t->base, header, sizeof(header));
    UartWrite(&t->base, chunk, chunk_len);
    UartWrite(&t->base, &check, 1);
}

/**
 * Frames one CMD_UNICAST_DATA_EX_REQ packet as {0x02, 0x07, len_lo, len_hi,
 * address[0..3], chunk[0..chunk_len), xor_checksum} - ctx is the 4-byte
 * destination address, repeated in full on every packet (each packet is
 * its own complete, independently addressed frame; there is no fragment
 * reassembly on the wire). Only ever called with chunk_len <=
 * THYONEI_MAX_PAYLOAD - 4, as guaranteed by ThyoneISendToAddress()
 * passing that as max_chunk, so address + chunk together never exceed
 * THYONEI_MAX_PAYLOAD.
 */
static void ThyoneISendToAddress_OnePacket(ThyoneI *t, void *ctx, uint8_t *chunk, uint16_t chunk_len){
    uint8_t *address = (uint8_t *)ctx;
    uint16_t length = (uint16_t)(chunk_len + 4);
    uint8_t header[4] = {
        0x02,
        0x07,
        (uint8_t)(length & 0xFF),
        (uint8_t)((length >> 8) & 0xFF),
    };

    uint8_t check = ThyoneIChecksum(0, header, sizeof(header));
    check = ThyoneIChecksum(check, address, 4);
    check = ThyoneIChecksum(check, chunk, chunk_len);

    UartWrite(&t->base, header, sizeof(header));
    UartWrite(&t->base, address, 4);
    UartWrite(&t->base, chunk, chunk_len);
    UartWrite(&t->base, &check, 1);
}

/**
 * Broadcasts msg over the radio, splitting it into as many packets of at
 * most THYONEI_MAX_PAYLOAD bytes as needed (see ThyoneI_SendFragmented()) -
 * payload_len can be arbitrarily long, not just up to what one Thyone-I
 * packet can carry.
 */
void ThyoneIBroadcast(ThyoneI *t, uint8_t *msg, uint16_t payload_len){
    ThyoneI_SendFragmented(t, msg, payload_len, THYONEI_MAX_PAYLOAD, NULL, ThyoneIBroadcast_OnePacket);
}

/**
 * Sends msg to a specific 4-byte destination address, splitting it into
 * as many packets as needed - each packet carries the full address (see
 * ThyoneISendToAddress_OnePacket()), so address is capped out of
 * max_chunk rather than merged into one buffer with msg first (no VLA,
 * no risk of a malformed frame on any but the first fragment).
 */
void ThyoneISendToAddress(ThyoneI *t, uint8_t *address, uint8_t *msg, uint16_t payload_len) {
    ThyoneI_SendFragmented(t, msg, payload_len, THYONEI_MAX_PAYLOAD - 4, address, ThyoneISendToAddress_OnePacket);
}



void ThyoneIRead(ThyoneI *t, uint8_t *out, uint len){
    UartRead(&t->base, out, len);
}