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

#ifndef OBSIDIAN_MINECRAFT_PROTOCOL_H
#define OBSIDIAN_MINECRAFT_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define MINECRAFT_USERNAME_LENGTH 16

#define MC_TRUE 0x01
#define MC_FALSE 0x00

/// Signed integer, 8 bits; two's complement.
typedef int8_t mc_byte;

/// Signed integer, 16 bits; two's complement.
typedef int16_t mc_word;

/// Signed integer, 32 bits; two's complement.
typedef int32_t mc_dword;

/// Signed integer, 64 bits; two's complement.
typedef int64_t mc_qword;

/// Single precision IEEE-754 floating point decimal.
typedef float mc_float;

/// Double precision IEEE-754 floating point decimal.
typedef double mc_double;

/// UTF-8 character string.
typedef char mc_utf8_char;

/// UCS-2 character string.
typedef wchar_t mc_ucs2_char;

/// Boolean value which can either be MC_TRUE or MC_FALSE.
typedef int8_t mc_bool;


/*!
 * Minecraft protocol packet IDs.
 */
enum mc_proto_packet_type {
    MC_PACKET_HEARTBEAT        = 0x00,
    MC_PACKET_AUTHENTICATION   = 0x01,
    MC_PACKET_HANDSHAKE        = 0x02,
    MC_PACKET_PLAYER_GROUNDED  = 0x0A,
    MC_PACKET_PLAYER_TRANSFORM = 0x0D,
    MC_PACKET_CHUNK            = 0x32,
};


/*!
 * Heartbeat package sent by the client to keep the connection alive. The server must respond to this packet with a
 * heartbeat of their own.
 *
 * \note Sent by both the client and the server.
 */
struct mc_proto_heartbeat {};


int mc_proto_encode_heartbeat(void* buffer, size_t buffer_size, struct mc_proto_heartbeat const* heartbeat);

int mc_proto_decode_heartbeat(void const* buffer, size_t buffer_size, struct mc_proto_heartbeat* heartbeat);


/*!
 * Second packet sent by the client to finalize the handshaking process.
 *
 * \note Sent by the client only.
 */
struct mc_proto_authentication_request {
    mc_dword protocol_version;
    mc_word username_length;
    mc_utf8_char username[MINECRAFT_USERNAME_LENGTH];
    mc_word password_length;
    mc_utf8_char password[32];
};


/*!
 * Decodes a buffer into an authentication request packet.
 * \see mc_proto_decode_client_packet()
 */
int mc_proto_decode_authentication_request(void const* buffer, size_t buffer_size,
                                           struct mc_proto_authentication_request* request);


/*!
 * Sent by the server in response to an authentication request.
 *
 * \note Sent by the server only.
 */
struct mc_proto_authentication_response {
    /// Looks to be some kind of ID, perhaps the entity assigned to the player?
    mc_dword entity_id;

    /// Length of unknown0.
    mc_word unknown0_length;

    /// Unknown. The official server sends an empty string.
    mc_utf8_char* unknown0;

    /// Length of unknown1.
    mc_word unknown1_length;

    /// Unknown. The official server sends an empty string.
    mc_utf8_char* unknown1;
};


/*!
 * Encodes an authentication response into a buffer.
 * \see mc_proto_encode_server_packet()
 */
int mc_proto_encode_authentication_response(void* buffer, size_t buffer_size,
                                            struct mc_proto_authentication_response const* response);


/*!
 * First packet sent by the client to begin the handshaking process.
 *
 * \note Sent by the client only.
 */
struct mc_proto_handshake_request {
    /// Length of the username, maximum value of 16.
    mc_word name_length;

    /// Username string.
    mc_utf8_char name[MINECRAFT_USERNAME_LENGTH];
};


/*!
 * Decodes a buffer into a handshake request packet.
 * \see mc_proto_decode_client_packet()
 */
int mc_proto_decode_handshake_request(void const* buffer, size_t buffer_size,
                                      struct mc_proto_handshake_request* request);


/*!
 * Packet sent by the server to respond to a handshake request.
 *
 * \note Sent by the server only.
 */
struct mc_proto_handshake_response {
    /// Length of the string.
    mc_word unknown_length;

    /// Seems to always be "-" in offline mode. Unknown what this is.
    mc_utf8_char* unknown;
};


/*!
 * Encodes a handshake response into a buffer.
 * \see mc_proto_encode_server_packet()
 */
int mc_proto_encode_handshake_response(void* buffer, size_t buffer_size,
                                       struct mc_proto_handshake_response const* response);


/*!
 * Message containing information about whether player is on the ground or falling.
 * \note Sent by the client.
 */
struct mc_proto_player_grounded {
    /// MC_FALSE if the player is falling, MC_TRUE if the player is on the ground.
    mc_bool grounded;
};


/*!
 * Decodes a transform packet.
 * \see mc_proto_decode_client_packet()
 */
int mc_proto_decode_player_grounded(void const* buffer, size_t buffer_size,
                                    struct mc_proto_player_grounded* grounded);


/*!
 * Message containing a full update on the player's position.
 * \note Sent by the client.
 */
struct mc_proto_player_transform {
    /// X coordinate of the player in world space.
    mc_double x;

    /// Y coordinate of the player in world space.
    mc_double y;

    /// Y coordinate of the player's head in world space.
    mc_double head_y;

    /// Z coordinate of the player in world space.
    mc_double z;

    /// Rotation of the player's character.
    mc_float yaw;

    /// Angle of the player's head.
    mc_float pitch;

    /// Whether the character is on the ground or not.
    mc_bool grounded;
};


/*!
 * Decodes a transform packet.
 * \see mc_proto_decode_client_packet()
 */
int mc_proto_decode_player_transform(void const* buffer, size_t buffer_size,
                                     struct mc_proto_player_transform* transform);


struct mc_proto_chunk {
    mc_dword x;
    mc_dword z;
    mc_bool unknown;
};


int mc_proto_encode_chunk(void* buffer, size_t buffer_size, struct mc_proto_chunk const* chunk);


/*!
 * Struct containing all packets the client can send to the server.
 */
struct mc_proto_client_packet {
    /// Packet identifier, refer to mc_proto_packet_type.
    mc_byte type;


    /// Anonymous union of packet data.
    union {
        struct mc_proto_heartbeat heartbeat;
        struct mc_proto_authentication_request authentication;
        struct mc_proto_handshake_request handshake;
        struct mc_proto_player_grounded grounded;
        struct mc_proto_player_transform transform;
    };
};


/*!
 * Decodes a packet received from the Minecraft client.
 * \param[in] buffer The buffer containing data to be decoded.
 * \param[in] buffer_size Size of the buffer in bytes.
 * \param[out] packet Pointer to a client packet structure to which the result is written.
 * \return The function can return one of the following:
 *         - <0 indicates the data is incomplete; the value represents how many more bytes are needed.
 *         - =0 indicates there was an error decoding the data.
 *         - >0 indicates the data was successfully read and decoded; the value represents the amount of bytes read.
 * \note The buffer is assumed to be network byte order.
 */
int mc_proto_decode_client_packet(void const* buffer, size_t buffer_size, struct mc_proto_client_packet* packet);


struct mc_proto_server_packet {
    /// Packet identifier, refer to mc_packet_type.
    mc_byte type;


    /// Anonymous union of packet data.
    union {
        struct mc_proto_heartbeat heartbeat;
        struct mc_proto_authentication_response authentication;
        struct mc_proto_handshake_response handshake;
        struct mc_proto_chunk chunk;
    };
};


/*!
 * Encodes a handshake response into a buffer.
 * \param[out] buffer Destination buffer to write the result to.
 * \param[in] buffer_size Size of the buffer in bytes.
 * \param[in] packet Pointer to a handshake response packet to encode.
 * \return The function can return one of the following:
 *         - <0 means that the buffer was too small; the value will be the needed buffer size.
 *         - =0 indicates an error.
 *         - >0 returns how many bytes were written to the buffer.
 * \note The resulting buffer will be in network byte order.
 */
int mc_proto_encode_server_packet(void* buffer, size_t buffer_size, struct mc_proto_server_packet const* packet);

#endif // !OBSIDIAN_MINECRAFT_PROTOCOL_H
