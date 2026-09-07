/**
 * @file thyonei.c
 * @brief Implementation of ThyoneI. The RX interrupt (see uart.c) only
 * ever fills Uart's raw rx_ring - all the actual Thyone-I framing
 * (Start/Command/Length/content/CS) happens here, in ThyoneI_Behave(),
 * ThyoneI's own periodic background task. It sorts every complete frame
 * it finds into one of two places: CMD_DATA_CNF/CMD_TXCOMPLETE_RSP
 * update the confirmation flags a send function is waiting on;
 * everything else (CMD_DATA_IND, in practice) is decoded into the
 * inbox for ThyoneIReceive() to hand out later.
 */

#include "thyonei.h"
#include <stdlib.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"

/** @brief How long a send waits for both confirmations before giving up. */
#define THYONEI_TXCOMPLETE_TIMEOUT_MS 150
/** @brief How long ThyoneI_ReadFrame() waits for a frame's content once its header is seen. */
#define THYONEI_FRAME_TIMEOUT_MS 50
/** @brief Largest complete frame ThyoneI_Behave() can parse in one go (header + max content + CS). */
#define THYONEI_MAX_FRAME (4 + 4 + 1 + THYONEI_MAX_PAYLOAD + 1)

static void ThyoneI_Behave(Agent *self);

ThyoneI *NewThyoneI(char *name, int timeout, uint32_t uxStackDepth, UBaseType_t uxPriority, uart_inst_t *uart, uint tx, uint rx, uint baudrate){
    ThyoneI *t = malloc(sizeof(ThyoneI));
    t->inbox_head = 0;
    t->inbox_count = 0;
    t->cnf_ready = false;
    t->tx_complete_ready = false;
    Uart_Init(&t->base, uart, tx, rx, baudrate, ThyoneI_Behave, Uart_Delete, name, timeout, uxStackDepth, uxPriority);
    return t;
}

/** @copydoc ThyoneIChecksum */
uint8_t ThyoneIChecksum(uint8_t acc, uint8_t *bytes, uint16_t len){
    for (uint16_t i = 0; i < len; i++) acc ^= bytes[i];
    return acc;
}

/** @copydoc ThyoneI_WaitForBytes */
bool ThyoneI_WaitForBytes(ThyoneI *t, uint8_t *out, uint len, uint32_t timeoutMs){
    uint got = 0;
    uint32_t waited = 0;
    while (got < len) {
        got += UartRead(&t->base, out + got, len - got);
        if (got >= len) {
            return true;
        }
        if (waited >= timeoutMs) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(THYONEI_WAIT_POLL_MS));
        waited += THYONEI_WAIT_POLL_MS;
    }
    return true;
}

/**
 * @brief Reads one complete Thyone-I frame (Start, Command, Length,
 * Length bytes of content, CS) out of rx_ring, if one is available.
 * Every Thyone-I frame - request, confirmation or indication - shares
 * this same envelope (see the user manual, "Command structure"), so
 * this one reader covers all of them; ThyoneI_DispatchFrame() decides
 * what a frame means from its Command byte.
 *
 * The header wait uses headerTimeoutMs (0 for "don't wait, just check
 * once" - what the background task uses so it never blocks when there
 * is nothing to read); once a header is found, the content is always
 * waited for with THYONEI_FRAME_TIMEOUT_MS, since the rest of an
 * already-started frame should follow almost immediately.
 *
 * @param t ThyoneI to read from.
 * @param out Destination buffer.
 * @param out_cap Capacity of out, in bytes.
 * @param out_len Set to the frame's total length on success.
 * @param headerTimeoutMs How long to wait for a frame to start.
 * @return bool True if a complete frame was read into out; false if no
 * frame started within headerTimeoutMs, its content didn't finish
 * within THYONEI_FRAME_TIMEOUT_MS, or it didn't fit in out_cap (the
 * overflow is still drained from rx_ring so the stream stays in sync).
 */
static bool ThyoneI_ReadFrame(ThyoneI *t, uint8_t *out, uint16_t out_cap, uint16_t *out_len, uint32_t headerTimeoutMs){
    uint8_t header[4];
    if (!ThyoneI_WaitForBytes(t, header, sizeof(header), headerTimeoutMs)) {
        return false;
    }

    uint16_t content_len = (uint16_t)header[2] | ((uint16_t)header[3] << 8);
    uint16_t total_len = (uint16_t)(4 + content_len + 1);

    if (total_len > out_cap) {
        uint8_t scratch[32];
        uint16_t remaining = (uint16_t)(content_len + 1);
        while (remaining > 0) {
            uint16_t chunk = remaining > sizeof(scratch) ? sizeof(scratch) : remaining;
            if (!ThyoneI_WaitForBytes(t, scratch, chunk, THYONEI_FRAME_TIMEOUT_MS)) {
                break;
            }
            remaining = (uint16_t)(remaining - chunk);
        }
        return false;
    }

    memcpy(out, header, sizeof(header));
    if (!ThyoneI_WaitForBytes(t, out + 4, (uint)(content_len + 1), THYONEI_FRAME_TIMEOUT_MS)) {
        return false;
    }
    *out_len = total_len;
    return true;
}

/**
 * @brief Decodes a CMD_DATA_IND frame and pushes it into t's inbox,
 * dropping it if the inbox is already full (oldest messages are handed
 * out first via ThyoneIReceive(); nothing here blocks).
 */
static void ThyoneI_PushInbox(ThyoneI *t, uint8_t *frame){
    uint16_t content_len = (uint16_t)frame[2] | ((uint16_t)frame[3] << 8);
    if (content_len < 5) {
        return; // malformed: src addr (4) + rssi (1) is the minimum
    }
    uint16_t payload_len = (uint16_t)(content_len - 5);
    if (payload_len > THYONEI_MAX_PAYLOAD) {
        payload_len = THYONEI_MAX_PAYLOAD; // defensive clamp, shouldn't happen
    }

    taskENTER_CRITICAL();
    if (t->inbox_count < THYONEI_INBOX_DEPTH) {
        uint8_t slot = (uint8_t)((t->inbox_head + t->inbox_count) % THYONEI_INBOX_DEPTH);
        ThyoneIMessage *m = &t->inbox[slot];
        m->src_address[0] = frame[4];
        m->src_address[1] = frame[5];
        m->src_address[2] = frame[6];
        m->src_address[3] = frame[7];
        m->rssi = (int8_t)frame[8];
        m->payload_len = payload_len;
        memcpy(m->payload, frame + 9, payload_len);
        t->inbox_count++;
    }
    taskEXIT_CRITICAL();
}

/**
 * @brief Routes one complete frame read by ThyoneI_Behave(): a
 * CMD_DATA_CNF (0x44) or CMD_TXCOMPLETE_RSP (0xC4) updates the matching
 * confirmation flag/status for whichever send is currently waiting
 * (ThyoneI_WaitForTxComplete()); a CMD_DATA_IND (0x84) goes into the
 * inbox; anything else is ignored.
 */
static void ThyoneI_DispatchFrame(ThyoneI *t, uint8_t *frame){
    uint8_t command = frame[1];
    if (command == 0x44) {
        taskENTER_CRITICAL();
        t->cnf_status = frame[4];
        t->cnf_ready = true;
        taskEXIT_CRITICAL();
    } else if (command == 0xC4) {
        taskENTER_CRITICAL();
        t->tx_complete_status = frame[4];
        t->tx_complete_ready = true;
        taskEXIT_CRITICAL();
    } else if (command == 0x84) {
        ThyoneI_PushInbox(t, frame);
    }
    // any other command: not something this project acts on yet, ignored
}

/**
 * @brief ThyoneI's periodic background task: drains and parses every
 * complete frame currently sitting in rx_ring (there may be more than
 * one per tick) and routes each one via ThyoneI_DispatchFrame(). This
 * is what lets CMD_DATA_IND messages be received into the inbox even
 * when nothing is being sent - see the @brief on ThyoneI in thyonei.h.
 */
static void ThyoneI_Behave(Agent *self) {
    ThyoneI *t = (ThyoneI *)self;
    uint8_t frame[THYONEI_MAX_FRAME];
    uint16_t frame_len;
    while (ThyoneI_ReadFrame(t, frame, sizeof(frame), &frame_len, 0)) {
        ThyoneI_DispatchFrame(t, frame);
    }
}

/**
 * @brief Waits for the CMD_DATA_CNF + CMD_TXCOMPLETE_RSP pair the
 * Thyone-I sends after every CMD_BROADCAST_DATA_REQ/CMD_UNICAST_DATA_EX_REQ
 * packet, as routed by ThyoneI_Behave() into cnf_ready/tx_complete_ready -
 * this function never touches the UART itself.
 *
 * @return bool True if both confirmations arrived within the timeout
 * and both reported success (status byte 0x00).
 */
static bool ThyoneI_WaitForTxComplete(ThyoneI *t){
    taskENTER_CRITICAL();
    t->cnf_ready = false;
    t->tx_complete_ready = false;
    taskEXIT_CRITICAL();

    bool cnf_done = false, cnf_ok = false;
    bool tx_done = false, tx_ok = false;
    uint32_t waited = 0;
    while (waited < THYONEI_TXCOMPLETE_TIMEOUT_MS) {
        taskENTER_CRITICAL();
        if (t->cnf_ready && !cnf_done) {
            cnf_ok = t->cnf_status == 0x00;
            cnf_done = true;
        }
        if (t->tx_complete_ready && !tx_done) {
            tx_ok = t->tx_complete_status == 0x00;
            tx_done = true;
        }
        taskEXIT_CRITICAL();

        if (cnf_done && tx_done) {
            return cnf_ok && tx_ok;
        }
        vTaskDelay(pdMS_TO_TICKS(THYONEI_WAIT_POLL_MS));
        waited += THYONEI_WAIT_POLL_MS;
    }
    return false;
}

/** @copydoc ThyoneI_SendFragmented */
bool ThyoneI_SendFragmented(ThyoneI *t, uint8_t *data, uint16_t len, uint16_t max_chunk, void *ctx, ThyoneISendFn sendOne){
    uint16_t offset = 0;
    while (offset < len) {
        uint16_t remaining = len - offset;
        uint16_t chunk_len = remaining > max_chunk ? max_chunk : remaining;
        if (!sendOne(t, ctx, data + offset, chunk_len)) {
            return false;
        }
        offset += chunk_len;
    }
    return true;
}

/**
 * Frames one packet as {0x02, 0x06, len_lo, len_hi, chunk[0..chunk_len), xor_checksum},
 * writes it out over uart one piece at a time - the checksum is
 * accumulated on the fly instead of assembling the whole frame in a
 * buffer first - then waits for its confirmation. Only ever called with
 * chunk_len <= THYONEI_MAX_PAYLOAD, as guaranteed by
 * ThyoneI_SendFragmented(). ctx is unused - broadcast needs nothing
 * beyond the chunk itself.
 */
static bool ThyoneIBroadcast_OnePacket(ThyoneI *t, void *ctx, uint8_t *chunk, uint16_t chunk_len){
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

    return ThyoneI_WaitForTxComplete(t);
}

/**
 * Frames one CMD_UNICAST_DATA_EX_REQ packet as {0x02, 0x07, len_lo, len_hi,
 * address[0..3], chunk[0..chunk_len), xor_checksum} - ctx is the 4-byte
 * destination address, repeated in full on every packet (each packet is
 * its own complete, independently addressed frame; there is no fragment
 * reassembly on the wire) - then waits for its confirmation. Only ever
 * called with chunk_len <= THYONEI_MAX_PAYLOAD - 4, as guaranteed by
 * ThyoneISendToAddress() passing that as max_chunk, so address + chunk
 * together never exceed THYONEI_MAX_PAYLOAD.
 */
static bool ThyoneISendToAddress_OnePacket(ThyoneI *t, void *ctx, uint8_t *chunk, uint16_t chunk_len){
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

    return ThyoneI_WaitForTxComplete(t);
}

/** @copydoc ThyoneIBroadcast */
bool ThyoneIBroadcast(ThyoneI *t, uint8_t *msg, uint16_t payload_len){
    return ThyoneI_SendFragmented(t, msg, payload_len, THYONEI_MAX_PAYLOAD, NULL, ThyoneIBroadcast_OnePacket);
}

/** @copydoc ThyoneISendToAddress */
bool ThyoneISendToAddress(ThyoneI *t, uint8_t *address, uint8_t *msg, uint16_t payload_len) {
    return ThyoneI_SendFragmented(t, msg, payload_len, THYONEI_MAX_PAYLOAD - 4, address, ThyoneISendToAddress_OnePacket);
}

/** @copydoc ThyoneIReceive */
bool ThyoneIReceive(ThyoneI *t, ThyoneIMessage *out){
    bool got = false;
    taskENTER_CRITICAL();
    if (t->inbox_count > 0) {
        *out = t->inbox[t->inbox_head];
        t->inbox_head = (uint8_t)((t->inbox_head + 1) % THYONEI_INBOX_DEPTH);
        t->inbox_count--;
        got = true;
    }
    taskEXIT_CRITICAL();
    return got;
}
