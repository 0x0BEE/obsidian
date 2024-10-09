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
#include "obsidian/log.h"

#include <assert.h>
#include <endian.h>
#include <string.h>

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

static inline void encode_dword(uint8_t* buf, mc_dword x, size_t* cursor) {
    x = htobe32(x);
    memcpy(buf + *cursor, &x, sizeof(mc_dword));
    *cursor += sizeof(mc_dword);
}

static inline mc_dword decode_dword(uint8_t const* buf, size_t* cursor) {
    mc_dword const x = be32toh(*(const uint32_t*)(buf + *cursor));
    *cursor += sizeof(mc_dword);
    return x;
}

static inline void encode_qword(uint8_t* buf, mc_qword x, size_t* cursor) {
    x = htobe64(x);
    memcpy(buf + *cursor, &x, sizeof(mc_qword));
    *cursor += sizeof(mc_qword);
}

static inline mc_qword decode_qword(uint8_t const* buf, size_t* cursor) {
    mc_qword const x = be64toh(*(const uint64_t*)(buf + *cursor));
    *cursor += sizeof(mc_qword);
    return x;
}

static inline void encode_float(uint8_t* buf, mc_float const x, size_t* cursor) {
    mc_dword n;
    memcpy(&n, &x, sizeof(mc_float));
    encode_dword(buf, n, cursor);
}

static inline mc_float decode_float(uint8_t const* buf, size_t* cursor) {
    mc_float x = 0.0f;
    mc_dword const n = decode_dword(buf, cursor);
    memcpy(&x, &n, sizeof(mc_float));
    return x;
}

static inline void encode_double(uint8_t* buf, mc_double const x, size_t* cursor) {
    mc_qword n;
    memcpy(&n, &x, sizeof(mc_double));
    encode_qword(buf, n, cursor);
}

static inline mc_double decode_double(uint8_t const* buf, size_t* cursor) {
    mc_double x = 0.0f;
    mc_qword const n = decode_qword(buf, cursor);
    memcpy(&x, &n, sizeof(mc_double));
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

static inline void encode_byte_array(uint8_t* dst, mc_byte const* str, uint16_t const len, size_t* cursor) {
    memcpy(dst + *cursor, str, len);
    *cursor += len;
}

int mc_proto_encode_heartbeat(void* buffer, size_t buffer_size, struct mc_proto_heartbeat const* heartbeat) {
    assert(heartbeat != NULL);
    size_t const needed = sizeof(mc_byte);
    ASSERT_BUFFER_SIZE(buffer_size, needed);
    assert(buffer != NULL);
    size_t cursor = 0;
    encode_byte(buffer, MC_PACKET_HEARTBEAT, &cursor);
    return cursor;
}

int mc_proto_decode_heartbeat(void const* buffer, size_t const buffer_size, struct mc_proto_heartbeat* heartbeat) {
    assert(buffer != NULL);
    assert(heartbeat != NULL);
    ASSERT_BUFFER_SIZE(buffer_size, 1);
    size_t cursor = 0;
    mc_byte const type = decode_byte(buffer, &cursor);
    if (type != MC_PACKET_HEARTBEAT) {
        return 0;
    }
    return cursor;
}

int mc_proto_decode_authentication_request(void const* buffer, size_t buffer_size,
                                           struct mc_proto_authentication_request* request) {
    assert(buffer != NULL);
    assert(request != NULL);
    ASSERT_BUFFER_SIZE(buffer_size, 7);
    size_t cursor = 0;
    mc_byte const type = decode_byte(buffer, &cursor);
    if (type != MC_PACKET_AUTHENTICATION) {
        return 0;
    }
    // Read and sanitize the username length.
    request->protocol_version = decode_dword(buffer, &cursor);
    request->username_length = decode_word(buffer, &cursor);
    if (request->username_length > MINECRAFT_USERNAME_LENGTH) {
        OBS_LOG_WARN("protocol", "Received username length > 16. This is invalid data!");
        return 0;
    }
    ASSERT_BUFFER_SIZE(buffer_size, cursor + request->username_length + sizeof(mc_word));
    decode_utf8_string(request->username, buffer, request->username_length, &cursor);
    request->password_length = decode_word(buffer, &cursor);
    if (request->password_length > 32) {
        OBS_LOG_WARN("protocol", "Received password length > 32. This is invalid data!");
        return 0;
    }
    decode_utf8_string(request->password, buffer, request->password_length, &cursor);
    return cursor;
}

int mc_proto_encode_authentication_response(void* buffer, size_t buffer_size,
                                            struct mc_proto_authentication_response const* response) {
    assert(response != NULL);
    size_t const needed = sizeof(mc_byte)
                          + sizeof(mc_dword)
                          + sizeof(mc_word) + sizeof(mc_utf8_char) * response->unknown0_length
                          + sizeof(mc_word) + sizeof(mc_utf8_char) * response->unknown1_length;
    ASSERT_BUFFER_SIZE(buffer_size, needed);
    assert(buffer != NULL);
    size_t cursor = 0;
    encode_byte(buffer, MC_PACKET_AUTHENTICATION, &cursor);
    encode_dword(buffer, response->entity_id, &cursor);
    encode_word(buffer, response->unknown0_length, &cursor);
    encode_utf8_string(buffer, response->unknown0, response->unknown0_length, &cursor);
    encode_word(buffer, response->unknown1_length, &cursor);
    encode_utf8_string(buffer, response->unknown1, response->unknown1_length, &cursor);
    return cursor;
}

int mc_proto_decode_handshake_request(void const* buffer, size_t const buffer_size,
                                      struct mc_proto_handshake_request* request) {
    assert(buffer != NULL);
    assert(request != NULL);
    ASSERT_BUFFER_SIZE(buffer_size, 3);
    size_t cursor = 0;
    mc_byte const type = decode_byte(buffer, &cursor);
    if (type != MC_PACKET_HANDSHAKE) {
        return 0;
    }
    // Read the username length, but sanitize this value.
    request->name_length = decode_word(buffer, &cursor);
    if (request->name_length > MINECRAFT_USERNAME_LENGTH) {
        OBS_LOG_WARN("protocol", "Received name length > 16. This is invalid data!");
        return 0;
    }
    ASSERT_BUFFER_SIZE(buffer_size, cursor + request->name_length);
    decode_utf8_string(request->name, buffer, request->name_length, &cursor);
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

int mc_proto_encode_time(void* buffer, size_t buffer_size, struct mc_proto_time const* time) {
    assert(time != NULL);
    size_t const needed = sizeof(mc_byte) + sizeof(mc_qword);
    ASSERT_BUFFER_SIZE(buffer_size, needed);
    assert(buffer != NULL);
    size_t cursor = 0;
    encode_byte(buffer, MC_PACKET_TIME, &cursor);
    encode_qword(buffer, time->time, &cursor);
    return cursor;
}

int mc_proto_decode_player_grounded(void const* buffer, size_t const buffer_size,
                                    struct mc_proto_player_grounded* grounded) {
    assert(buffer != NULL);
    assert(grounded != NULL);
    ASSERT_BUFFER_SIZE(buffer_size, sizeof(mc_byte) + sizeof(mc_bool));
    size_t cursor = 0;
    mc_byte const type = decode_byte(buffer, &cursor);
    if (type != MC_PACKET_PLAYER_GROUNDED) {
        return 0;
    }
    grounded->grounded = decode_byte(buffer, &cursor);
    return cursor;
}

int mc_proto_decode_player_position(void const* buffer, size_t const buffer_size,
                                    struct mc_proto_player_position* position) {
    assert(buffer != NULL);
    assert(position != NULL);
    ASSERT_BUFFER_SIZE(buffer_size, sizeof(mc_byte) + sizeof(mc_double) * 4 + sizeof(mc_bool));
    size_t cursor = 0;
    mc_byte const type = decode_byte(buffer, &cursor);
    if (type != MC_PACKET_PLAYER_POSITION) {
        return 0;
    }
    position->x = decode_double(buffer, &cursor);
    position->y = decode_double(buffer, &cursor);
    position->head_y = decode_double(buffer, &cursor);
    position->z = decode_double(buffer, &cursor);
    position->grounded = decode_byte(buffer, &cursor);
    return cursor;
}

int mc_proto_decode_player_rotation(void const* buffer, size_t const buffer_size,
    struct mc_proto_player_rotation* rotation) {
    assert(buffer != NULL);
    assert(rotation != NULL);
    ASSERT_BUFFER_SIZE(buffer_size, sizeof(mc_byte) + sizeof(mc_float) * 2 + sizeof(mc_bool));
    size_t cursor = 0;
    mc_byte const type = decode_byte(buffer, &cursor);
    if (type != MC_PACKET_PLAYER_ROTATION) {
        return 0;
    }
    rotation->yaw = decode_float(buffer, &cursor);
    rotation->pitch = decode_float(buffer, &cursor);
    rotation->grounded = decode_byte(buffer, &cursor);
    return cursor;
}

int mc_proto_encode_player_transform(void* buffer, size_t const buffer_size, struct mc_proto_player_transform const* transform) {
    assert(transform != NULL);
    size_t const needed = sizeof(mc_byte) + sizeof(mc_double) * 4 + sizeof(mc_float) * 2 + sizeof(mc_bool);
    ASSERT_BUFFER_SIZE(buffer_size, needed);
    assert(buffer != NULL);
    size_t cursor = 0;
    encode_byte(buffer, MC_PACKET_PLAYER_TRANSFORM, &cursor);
    encode_double(buffer, transform->x, &cursor);
    // Order for y and head_y is inverted when sending to client.
    encode_double(buffer, transform->head_y, &cursor);
    encode_double(buffer, transform->y, &cursor);
    encode_double(buffer, transform->z, &cursor);
    encode_float(buffer, transform->yaw, &cursor);
    encode_float(buffer, transform->pitch, &cursor);
    encode_byte(buffer, transform->grounded, &cursor);
    return cursor;
}

int mc_proto_decode_player_transform(void const* buffer, size_t buffer_size,
                                     struct mc_proto_player_transform* transform) {
    assert(buffer != NULL);
    assert(transform != NULL);
    ASSERT_BUFFER_SIZE(buffer_size, sizeof(mc_byte) + sizeof(mc_double) * 4 + sizeof(mc_float) * 2 + sizeof(mc_bool));
    size_t cursor = 0;
    mc_byte const type = decode_byte(buffer, &cursor);
    if (type != MC_PACKET_PLAYER_TRANSFORM) {
        return 0;
    }
    transform->x = decode_double(buffer, &cursor);
    transform->y = decode_double(buffer, &cursor);
    transform->head_y = decode_double(buffer, &cursor);
    transform->z = decode_double(buffer, &cursor);
    transform->yaw = decode_float(buffer, &cursor);
    transform->pitch = decode_float(buffer, &cursor);
    transform->grounded = decode_byte(buffer, &cursor);
    return cursor;
}

int mc_proto_encode_chunk(void* buffer, size_t buffer_size, struct mc_proto_chunk const* chunk) {
    assert(chunk != NULL);
    size_t const needed = sizeof(mc_byte) + sizeof(mc_dword) * 2 + sizeof(mc_bool);
    ASSERT_BUFFER_SIZE(buffer_size, needed);
    assert(buffer != NULL);
    size_t cursor = 0;
    encode_byte(buffer, MC_PACKET_CHUNK, &cursor);
    encode_dword(buffer, chunk->x, &cursor);
    encode_dword(buffer, chunk->z, &cursor);
    encode_byte(buffer, chunk->initialize, &cursor);
    return cursor;
}

int mc_proto_encode_chunk_data(void* buffer, size_t buffer_size, struct mc_proto_chunk_data const* chunk_data) {
    assert(chunk_data != NULL);
    size_t const needed = sizeof(mc_byte) * 4 + sizeof(mc_dword) * 3 + sizeof(mc_word)
                          + sizeof(mc_byte) * chunk_data->compressed_size;
    ASSERT_BUFFER_SIZE(buffer_size, needed);
    assert(buffer != NULL);
    size_t cursor = 0;
    encode_byte(buffer, MC_PACKET_CHUNK_DATA, &cursor);
    encode_dword(buffer, chunk_data->x, &cursor);
    encode_word(buffer, chunk_data->y, &cursor);
    encode_dword(buffer, chunk_data->z, &cursor);
    encode_byte(buffer, chunk_data->x_size, &cursor);
    encode_byte(buffer, chunk_data->y_size, &cursor);
    encode_byte(buffer, chunk_data->z_size, &cursor);
    encode_dword(buffer, chunk_data->compressed_size, &cursor);
    encode_byte_array(buffer, chunk_data->data, chunk_data->compressed_size, &cursor);
    return cursor;
}

int mc_proto_decode_client_packet(void const* buffer, size_t const buffer_size, struct mc_proto_client_packet* packet) {
    assert(buffer != NULL);
    assert(packet != NULL);
    ASSERT_BUFFER_SIZE(buffer_size, 1);
    size_t cursor = 0;
    packet->type = decode_byte(buffer, &cursor);

    switch (packet->type) {
        case MC_PACKET_HEARTBEAT:
            return mc_proto_decode_heartbeat(buffer, buffer_size, &packet->heartbeat);

        case MC_PACKET_AUTHENTICATION:
            return mc_proto_decode_authentication_request(buffer, buffer_size, &packet->authentication);

        case MC_PACKET_HANDSHAKE:
            return mc_proto_decode_handshake_request(buffer, buffer_size, &packet->handshake);

        case MC_PACKET_PLAYER_GROUNDED:
            return mc_proto_decode_player_grounded(buffer, buffer_size, &packet->grounded);

        case MC_PACKET_PLAYER_POSITION:
            return mc_proto_decode_player_position(buffer, buffer_size, &packet->position);

        case MC_PACKET_PLAYER_ROTATION:
            return mc_proto_decode_player_rotation(buffer, buffer_size, &packet->rotation);

        case MC_PACKET_PLAYER_TRANSFORM:
            return mc_proto_decode_player_transform(buffer, buffer_size, &packet->transform);

        default:
            OBS_LOG_WARN("protocol", "Cannot decode packet with unknown type 0x%02X", packet->type);
            return 0;
    }
}

int mc_proto_encode_server_packet(void* buffer, size_t const buffer_size, struct mc_proto_server_packet const* packet) {
    assert(packet != NULL);

    switch (packet->type) {
        case MC_PACKET_HEARTBEAT:
            return mc_proto_encode_heartbeat(buffer, buffer_size, &packet->heartbeat);

        case MC_PACKET_AUTHENTICATION:
            return mc_proto_encode_authentication_response(buffer, buffer_size, &packet->authentication);

        case MC_PACKET_HANDSHAKE:
            return mc_proto_encode_handshake_response(buffer, buffer_size, &packet->handshake);

        case MC_PACKET_TIME:
            return mc_proto_encode_time(buffer, buffer_size, &packet->time);

        case MC_PACKET_PLAYER_TRANSFORM:
            return mc_proto_encode_player_transform(buffer, buffer_size, &packet->transform);

        case MC_PACKET_CHUNK:
            return mc_proto_encode_chunk(buffer, buffer_size, &packet->chunk);

        case MC_PACKET_CHUNK_DATA:
            return mc_proto_encode_chunk_data(buffer, buffer_size, &packet->chunk_data);

        default:
            OBS_LOG_WARN("protocol", "Cannot encode packet with unknown type 0x%02X", packet->type);
            return 0;
    }
}
