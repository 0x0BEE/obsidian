/*
 * Obsidian: a fast Minecraft server
 * Copyright (C) 2024  Jesse Gerard Brands
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "obsidian/minecraft/protocol.h"

#include <assert.h>
#include <endian.h>
#include <string.h>

#include "obsidian/log.h"

#define ASSERT_BUFFER_SIZE(have, want) \
    do { \
        if (have < want) { \
            return -(want - have); \
        } \
    } while(0)

static inline void encode_byte(uint8_t* buf, mc_byte const x, size_t* cursor) {
    buf[(*cursor)++] = x;
}

static inline mc_byte decode_byte(uint8_t const* buf, size_t* cursor) {
    return buf[(*cursor)++];
}

static inline void encode_word(uint8_t* buf, mc_word x, size_t* cursor) {
    x = htobe16(x);
    memcpy(buf + *cursor, &x, sizeof(mc_word));
    *cursor += sizeof(mc_word);
}

static inline mc_word decode_word(uint8_t const* buf, size_t* cursor) {
    mc_word const x = be16toh(*(const uint16_t*)(buf + *cursor));
    *cursor += sizeof(mc_word);
    return x;
}

static inline void encode_utf8_string(uint8_t* dst, mc_utf8_char const* str, uint16_t const len, size_t* cursor) {
    encode_word(dst, len, cursor);
    memcpy(dst + *cursor, str, len);
    *cursor += len;
}

static inline mc_utf8_char*
decode_utf8_string(mc_utf8_char* dst, uint8_t const* buf, size_t const len, size_t* cursor) {
    memcpy(dst, buf + *cursor, len);
    *cursor += len;
    return dst;
}

int mc_proto_decode_handshake_request(void const* buffer, size_t const buffer_size,
                                      struct mc_proto_handshake_request* packet) {
    assert(buffer != NULL);
    assert(packet != NULL);
    ASSERT_BUFFER_SIZE(buffer_size, 3);
    size_t cursor = 0;
    mc_byte const type = decode_byte(buffer, &cursor);
    if (type != MC_PACKET_HANDSHAKE) {
        return 0;
    }
    // Read the username length, but sanitize this value.
    packet->name_length = decode_word(buffer, &cursor);
    packet->name_length = packet->name_length > 16 ? 16 : packet->name_length;
    ASSERT_BUFFER_SIZE(buffer_size, cursor + packet->name_length);
    decode_utf8_string(packet->name, buffer, packet->name_length, &cursor);
    return cursor;
}

int mc_proto_encode_handshake_response(void* buffer, size_t const buffer_size,
                                       struct mc_proto_handshake_response const* response) {
    assert(response != NULL);
    size_t const needed = sizeof(mc_byte) + sizeof(mc_word) + sizeof(mc_utf8_char) * response->unknown_length;
    ASSERT_BUFFER_SIZE(buffer_size, needed);
    assert(buffer != NULL);
    size_t cursor = 0;
    encode_byte(buffer, MC_PACKET_HANDSHAKE, &cursor);
    encode_utf8_string(buffer, response->unknown, response->unknown_length, &cursor);
    return cursor;
}

int mc_proto_decode_client_packet(void const* buffer, size_t const buffer_size, struct mc_proto_client_packet* packet) {
    assert(buffer != NULL);
    assert(packet != NULL);
    ASSERT_BUFFER_SIZE(buffer_size, 1);
    size_t cursor = 0;
    packet->type = decode_byte(buffer, &cursor);

    switch (packet->type) {
        case MC_PACKET_HANDSHAKE:
            return mc_proto_decode_handshake_request(buffer, buffer_size, &packet->handshake);

        default:
            OBS_LOG_WARN("protocol", "Cannot decode packet with unknown type %d", packet->type);
            return 0;
    }
}

int mc_proto_encode_server_packet(void* buffer, size_t const buffer_size, struct mc_proto_server_packet const* packet) {
    assert(packet != NULL);

    switch (packet->type) {
        case MC_PACKET_HANDSHAKE:
            return mc_proto_encode_handshake_response(buffer, buffer_size, &packet->handshake);

        default:
            OBS_LOG_WARN("protocol", "Cannot encode packet with unknown type %d", packet->type);
            return 0;
    }
}
