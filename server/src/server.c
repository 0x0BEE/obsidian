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

#include "obsidian/server.h"
#include "obsidian/log.h"
#include "obsidian/memory.h"
#include "obsidian/minecraft/protocol.h"

#include <liburing.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>


/*!
 * A simple wrapper around obs_ring_buffer that keeps track of a read and write cursor.
 * \see obs_rw_buffer_size()
 * \see obs_rw_buffer_capacity()
 * \see obs_rw_buffer_read_ptr()
 * \see obs_rw_buffer_write_ptr()
 */
struct obs_rw_buffer {
    struct obs_ring_buffer* ring;
    size_t read_cursor;
    size_t write_cursor;
};


/*!
 * Gets the size of readable data in the buffer.
 * \param b Pointer to an obs_rw_buffer structure.
 * \return Size in bytes of readable data in the buffer.
 */
size_t obs_rw_buffer_size(struct obs_rw_buffer const* b) {
    return b->write_cursor - b->read_cursor;
}

/*!
 * Gets the size of the writeable space in this buffer.
 * \param b Pointer to an obs_rw_buffer structure.
 * \return Size in bytes of write capacity in the buffer.
 */
size_t obs_rw_buffer_capacity(struct obs_rw_buffer const* b) {
    return b->ring->size - obs_rw_buffer_size(b);
}

/*!
 * Gets a pointer to readable portion of the buffer.
 * \param b Pointer to an obs_rw_buffer structure.
 * \return Pointer to the readable memory in the buffer.
 * \note Call obs_rw_buffer_size() to find the length of the readable data.
 */
uint8_t* obs_rw_buffer_read_ptr(struct obs_rw_buffer const* b) {
    return b->ring->data + b->read_cursor % b->ring->size;
}

/*!
 * Gets a pointer to writeable portion of the buffer.
 * \param b Pointer to an obs_rw_buffer structure.
 * \return Pointer to the writeable memory in the buffer.
 * \note Call obs_rw_buffer_capacity() to find the length of the writeable memory.
 */
uint8_t* obs_rw_buffer_write_ptr(struct obs_rw_buffer const* b) {
    return obs_rw_buffer_read_ptr(b) + obs_rw_buffer_size(b);
}


/*!
 * Enumeration with status codes associated with a client session.
 */
enum obs_session_status {
    /// This session has disconnected.
    SESSION_DISCONNECTED = 0,

    /// This session is in the process of handshaking with the server.
    SESSION_HANDSHAKING = 1,

    /// The session is in the process of authenticating with the server.
    SESSION_AUTHENTICATING = 2,

    /// The session is connected and in-game.
    SESSION_CONNECTED = 3,

    /// This session is in the process of getting disconnected.
    SESSION_DISCONNECTING = 4,
};


/*!
 * Client session data.
 */
struct obs_session {
    /// File descriptor for the client connecting socket. If 0, the session is unused.
    int socket;

    /// One of obs_session_status.
    int status;

    size_t username_length;
    char username[16];

    /// Remote address of the connecting client.
    uint32_t address;

    /// Port of the connecting client.
    uint16_t port;

    /// Read ring buffer.
    struct obs_rw_buffer in;

    /// Total amount of bytes received from this client.
    size_t total_in;

    /// Total amount of bytes sent to this client.
    size_t total_out;
};


struct obs_server {
    /// File descriptor for the server socket.
    int socket;

    /// Array of client sessions.
    struct obs_session* sessions;

    /// Maximum amount of sessions (connections) the server can have at once.
    size_t session_limit;

    /// I/O operation ring buffers.
    struct io_uring ring;

    /// Pool allocator for packet frames.
    struct obs_pool_allocator* frame_allocator;
};


/*!
 * Finds the first unused client session.
 * \param server Pointer to a server structure.
 * \return Pointer to a client session structure, or NULL if no sessions are open.
 * \note If this function returns NULL, the server is at its connection limit.
 */
struct obs_session* obs_server_get_available_session(struct obs_server const* server) {
    OBS_LOG_TRACE("server", "Seeking unused session");
    for (size_t i = 0; i < server->session_limit; ++i) {
        struct obs_session* session = &server->sessions[i];
        if (session->socket == 0) {
            OBS_LOG_TRACE("server", "Found unused session at index %llu", i);
            return session;
        }
    }
    OBS_LOG_TRACE("server", "Could not find unused session! The server may be full.");
    return NULL;
}

/*!
 * Releases and resets a session, making it available for a new session.
 * \param session Pointer to a client session structure.
 */
void obs_session_release(struct obs_session* session) {
    OBS_LOG_TRACE("server", "Releasing session %08X:%d", session->address, session->port);
    obs_free_ring_buffer(session->in.ring);
    *session = (struct obs_session){0};
}

/*!
 * Allocates a buffer from the pool.
 * \param server Pointer to a server structure.
 * \param size Size of the buffer in bytes.
 * \return Pointer to the allocated memory, or NULL if out of memory.
 */
void* obs_server_get_buffer(struct obs_server const* server, size_t const size) {
    OBS_LOG_TRACE("server", "Allocating %llu byte(s) sized buffer", size);
    return malloc(size);
}

/*!
 * Releases a buffer back to the pool.
 * \param server Pointer to a server structure.
 * \param ptr Pointer to the alllocated memory to deallocate.
 */
void obs_server_release_buffer(struct obs_server const* server, void* ptr) {
    OBS_LOG_TRACE("server", "Releasing buffer");
    free(ptr);
}


/*!
 * Enumeration of packet frame types.
 */
enum obs_frame_type {
    /// Unknown packet frame.
    OBS_FRAME_UNKNOWN,

    /// Packet frame for a send() operation.
    OBS_FRAME_SEND,

    /// Packet frame for a recv() operation.
    OBS_FRAME_RECEIVE,

    /// Packet frame for an accept() operation.
    OBS_FRAME_ACCEPT,

    /// Packet frame for a close() operation.
    OBS_FRAME_CLOSE,
};


/*!
 * Gets a string representation of a packet frame type.
 * \param type One of obs_frame_type.
 * \return String representation of the type.
 */
static char const* obs_frame_type_to_string(enum obs_frame_type const type) {
    switch (type) {
        case OBS_FRAME_SEND:
            return "SEND";
        case OBS_FRAME_RECEIVE:
            return "RECEIVE";
        case OBS_FRAME_ACCEPT:
            return "ACCEPT";
        case OBS_FRAME_CLOSE:
            return "CLOSE";
        default:
            return "UNKNOWN";
    }
}


/*!
 * Packet frame associated with a SEND request.
 */
struct obs_send_frame {
    /// Buffer that is being sent to the client.
    void* buffer;

    /// Total size of the buffer.
    size_t buffer_size;

    /// Amount of bytes written to the client.
    size_t bytes_out;
};


/*!
 * Packet frame associated with a RECEIVE request.
 */
struct obs_receive_frame {
    /// Buffer that is being written to.
    void* buffer;

    /// Total size of the buffer in bytes.
    size_t buffer_size;

    /// Total bytes that have been received into the buffer so far.
    size_t bytes_in;
};


/*!
 * Packet frame data associated with an ACCEPT request.
 */
struct obs_accept_frame {
    /// Address of the client.
    struct sockaddr_in address;

    /// Length of the address.
    socklen_t address_length;
};


/*!
 * Packet frame data.
 *
 * \note These objects have the lifetime of a single packet. They are automatically allocated upon sending/receiving
 *       a new packet and are automatically destroyed when the packet is fully sent or received and processed.
 */
struct obs_frame {
    /// One of obs_frame_type.
    int type;

    /// Trace ID
    uint64_t trace;

    /// Pointer to the connection session.
    struct obs_session* session;


    /// Union of possible frame data types. See type for how to interpret this data.
    union {
        struct obs_send_frame send;
        struct obs_receive_frame receive;
        struct obs_accept_frame accept;
    };
};


static uint32_t trace_counter = 1;

/*!
 * Allocates a new packet frame.
 * \param server Pointer to a server structure.
 * \param session Pointer to a client session. May be NULL.
 * \param type One of obs_frame_type.
 * \return Pointer to the allocated packet frame.
 */
struct obs_frame* obs_server_create_frame(struct obs_server const* server, struct obs_session* session,
                                          enum obs_frame_type const type) {
    struct obs_frame* frame = obs_pool_allocator_alloc(server->frame_allocator);
    frame->type = type;
    frame->session = session;
    frame->trace = trace_counter++;
    OBS_LOG_TRACE("server", "Created new %s packet frame[%llu]", obs_frame_type_to_string(type), frame->trace);
    return frame;
}

/*!
 * Releases a packet frame back to the pool.
 * \param server Pointer to the server structure.
 * \param frame Pointer to the packet frame structure.
 */
void obs_server_release_frame(struct obs_server const* server, struct obs_frame* frame) {
    OBS_LOG_TRACE("server", "Destroying %s network frame[%llu]", obs_frame_type_to_string(frame->type), frame->trace);
    obs_pool_allocator_free(server->frame_allocator, frame);
}

/*!
 * Allocates a new SEND frame.
 * \param server Pointer to a server structure.
 * \param session Pointer to a client session.
 * \param buffer Pointer to the buffer associated with this send.
 * \param buffer_size Size of the buffer.
 * \return Pointer to the allocated packet frame.
 */
struct obs_frame* obs_frame_create_send(struct obs_server const* server, struct obs_session* session,
                                        void* buffer, size_t const buffer_size) {
    struct obs_frame* frame = obs_server_create_frame(server, session, OBS_FRAME_SEND);
    frame->send.buffer = buffer;
    frame->send.buffer_size = buffer_size;
    frame->send.bytes_out = 0;
    return frame;
}

/*!
 * Allocates a new RECEIVE frame.
 * \param server Pointer to a server structure.
 * \param session Pointer to a client session.
 * \param buffer Pointer to the buffer associated with this receive.
 * \param buffer_size Size of the buffer.
 * \return Pointer to the allocated packet frame.
 */
struct obs_frame* obs_frame_create_receive(struct obs_server const* server, struct obs_session* session,
                                           void* buffer, size_t const buffer_size) {
    struct obs_frame* frame = obs_server_create_frame(server, session, OBS_FRAME_RECEIVE);
    frame->receive.buffer = buffer;
    frame->receive.buffer_size = buffer_size;
    frame->receive.bytes_in = 0;
    return frame;
}

/*!
 * Allocates a new ACCEPT packet frame.
 * \param server Pointer to a server structure.
 * \param session Pointer to a client session. May be NULL.
 * \return Pointer to the allocated packet frame.
 */
struct obs_frame* obs_frame_create_accept(struct obs_server const* server, struct obs_session* session) {
    struct obs_frame* frame = obs_server_create_frame(server, session, OBS_FRAME_ACCEPT);
    frame->accept.address_length = sizeof(struct sockaddr_in);
    return frame;
}

/*!
 * Allocates a new CLOSE packet frame.
 * \param server Pointer to a server structure.
 * \param session Pointer to a client session. May be NULL.
 * \return Pointer to the allocated packet frame.
 */
struct obs_frame* obs_frame_create_close(struct obs_server const* server, struct obs_session* session) {
    struct obs_frame* frame = obs_server_create_frame(server, session, OBS_FRAME_CLOSE);
    return frame;
}

/*!
 * Submits enqueued I/O operations to the kernel.
 * \param server Pointer to a server structure.
 * \return The result of io_uring_submit().
 * \see io_uring_submit()
 */
int obs_server_submit_queue(struct obs_server* server) {
    OBS_LOG_TRACE("server", "Submitting I/O queue to kernel");
    return io_uring_submit(&server->ring);
}

void obs_server_queue_send(struct obs_server* server, struct obs_session* session,
                           int const socket, void* buffer, size_t const buffer_size, int const flags) {
    OBS_LOG_TRACE("server", "Queueing 'send' I/O operation");
    struct io_uring_sqe* sqe = io_uring_get_sqe(&server->ring);
    struct obs_frame* frame = obs_frame_create_send(server, session, buffer, buffer_size);
    io_uring_prep_send(sqe, socket, buffer, buffer_size, flags);
    io_uring_sqe_set_data(sqe, frame);
}

/*!
 * Queues a recv() operation to the I/O ring buffer.
 * \param server Pointer to a server structure.
 * \param session Pointer to a client session structure.
 * \param socket Socket file descriptor.
 * \param buffer Pointer to the buffer to receive into.
 * \param buffer_size Size of the buffer.
 * \param flags Flags to pass to recv().
 * \note This is an asynchronous function, to start the queued operation call obs_server_submit_queue().
 */
void obs_server_queue_recv(struct obs_server* server, struct obs_session* session,
                           int const socket, void* buffer, size_t const buffer_size, int const flags) {
    OBS_LOG_TRACE("server", "Queueing 'recv' I/O operation");
    struct io_uring_sqe* sqe = io_uring_get_sqe(&server->ring);
    struct obs_frame* frame = obs_frame_create_receive(server, session, buffer, buffer_size);
    io_uring_prep_recv(sqe, socket, buffer, buffer_size, flags);
    io_uring_sqe_set_data(sqe, frame);
}

/*!
 * Queues a recv() operation on the buffer with an offset to write into.
 * \param server Pointer to a server structure.
 * \param session Pointer to a client session structure.
 * \param socket Socket file descriptor.
 * \param buffer Pointer to the buffer to receive into.
 * \param buffer_size Size of the buffer.
 * \param offset Offset into the buffer to read at.
 * \param flags Flags to pass to recv().
 * \note This function does not allocate a new packet frame.
 */
void obs_server_queue_recv_offset(struct obs_server* server, struct obs_session* session, int const socket,
                                  void* buffer, size_t const buffer_size, size_t offset, int const flags) {
    OBS_LOG_TRACE("server", "Queueing 'recv' I/O operation for additional data");
    struct io_uring_sqe* sqe = io_uring_get_sqe(&server->ring);
    struct obs_frame* frame = obs_frame_create_receive(server, session, buffer, buffer_size);
    frame->receive.bytes_in = offset;
    io_uring_prep_recv(sqe, socket, buffer + offset, buffer_size - offset, flags);
    io_uring_sqe_set_data(sqe, frame);
}

/*!
 * Queues an accept() operation to the I/O ring buffer.
 * \param server Pointer to a server structure.
 * \param flags Flags to pass to accept().
 * \note This is an asynchronous function, to start the queued operation call obs_server_submit_queue().
 */
void obs_server_queue_accept(struct obs_server* server, int const flags) {
    OBS_LOG_TRACE("server", "Queueing 'accept' I/O operation");
    struct io_uring_sqe* sqe = io_uring_get_sqe(&server->ring);
    struct obs_frame* frame = obs_frame_create_accept(server, NULL);
    io_uring_prep_accept(sqe, server->socket,
                         (struct sockaddr*) &frame->accept.address,
                         &frame->accept.address_length, flags);
    io_uring_sqe_set_data(sqe, frame);
}

/*!
 * Queues a close() operation to the I/O ring buffer.
 * \param server Pointer to a server structure.
 * \param session Pointer to a client session structure.
 * \param fd File descriptor to close.
 * \note This is an asynchronous function, to start the queued operation call obs_server_submit_queue().
 */
void obs_server_queue_close(struct obs_server* server, struct obs_session* session, int const fd) {
    OBS_LOG_TRACE("server", "Queueing 'close' I/O operation");
    struct io_uring_sqe* sqe = io_uring_get_sqe(&server->ring);
    struct obs_frame* frame = obs_frame_create_close(server, session);
    io_uring_prep_close(sqe, fd);
    io_uring_sqe_set_data(sqe, frame);
}

void obs_server_heartbeat(struct obs_server* server, struct obs_session* session,
                          struct mc_proto_heartbeat const* heartbeat) {
    OBS_LOG_TRACE("server", "Received heartbeat from %.*s (%08X:%d)",
                  session->username_length, session->username, session->address, session->port);
    // This is, presumably, the keepalive packet. For now, let's just reply with an identical message.
    struct mc_proto_heartbeat const response = {};
    size_t const length = -mc_proto_encode_heartbeat(NULL, 0, &response);
    uint8_t* buffer = obs_server_get_buffer(server, length);
    mc_proto_encode_heartbeat(buffer, length, &response);
    obs_server_queue_send(server, session, session->socket, buffer, length, 0);
    obs_server_submit_queue(server);
}

void obs_server_authenticate(struct obs_server* server, struct obs_session* session,
                             struct mc_proto_authentication_request const* authentication) {
    OBS_LOG_DEBUG("server", "Handling authentication request from %08X:%d", session->address, session->port);
    if (session->status != SESSION_AUTHENTICATING) {
        OBS_LOG_WARN(
            "server", "Received authentication from %08X:%d, but session status is not AUTHENTICATING. Disconnecting!",
            session->address, session->port);
        obs_server_queue_close(server, session, session->socket);
        obs_server_submit_queue(server);
        return;
    }
    if (authentication->protocol_version != 1) {
        OBS_LOG_INFO("server", "Player %.*s (%08X:%d) is running incompatible protocol version %d. Disconnecting!",
                     session->username_length, session->username, session->address, session->port,
                     authentication->protocol_version);
        obs_server_queue_close(server, session, session->socket);
        obs_server_submit_queue(server);
        return;
    }
    session->status = SESSION_CONNECTED;
    // Send the response packet.
    OBS_LOG_DEBUG("server", "Sending authentication response to %.*s (%08X:%d)",
                  session->username_length, session->username, session->address, session->port);
    struct mc_proto_authentication_response const response = {
        .entity_id = 0,
        .unknown0_length = 0,
        .unknown0 = "",
        .unknown1_length = 0,
        .unknown1 = "",
    };
    size_t const length = -mc_proto_encode_authentication_response(NULL, 0, &response);
    uint8_t* buffer = obs_server_get_buffer(server, length);
    mc_proto_encode_authentication_response(buffer, length, &response);
    obs_server_queue_send(server, session, session->socket, buffer, length, 0);
    obs_server_submit_queue(server);
    OBS_LOG_INFO("server", "Player %.*s (%08X:%d) has joined the game",
                 session->username_length, session->username, session->address, session->port);
}

void obs_server_handshake(struct obs_server* server, struct obs_session* session,
                          struct mc_proto_handshake_request const* request) {
    OBS_LOG_DEBUG("server", "Handling handshake request from %08X:%d", session->address, session->port);
    if (session->status != SESSION_HANDSHAKING) {
        OBS_LOG_WARN("server", "Received handshake from %08X:%d, but session status is not HANDSHAKING. Disconnecting!",
                     session->address, session->port);
        obs_server_queue_close(server, session, session->socket);
        obs_server_submit_queue(server);
        return;
    }
    // Copy over the username of the player.
    session->username_length = request->name_length;
    memcpy(session->username, request->name, request->name_length);
    session->status = SESSION_AUTHENTICATING;
    // Send back to the appropriate response to the client.
    OBS_LOG_DEBUG("server", "Sending handshake response to %.*s (%08X:%d)",
                  session->username_length, session->username, session->address, session->port);
    struct mc_proto_handshake_response const response = {
        .unknown_length = 1,
        .unknown = "-",
    };
    size_t const length = -mc_proto_encode_handshake_response(NULL, 0, &response);
    uint8_t* buffer = obs_server_get_buffer(server, length);
    mc_proto_encode_handshake_response(buffer, length, &response);
    obs_server_queue_send(server, session, session->socket, buffer, length, 0);
    obs_server_submit_queue(server);
    OBS_LOG_INFO("server", "Player %.*s (%08X:%d) is joining the game",
                 session->username_length, session->username, session->address, session->port);
}

void obs_server_dispatch_packet(struct obs_server* server, struct obs_session* session,
                                struct mc_proto_client_packet const* packet) {
    switch (packet->type) {
        case MC_PACKET_HEARTBEAT:
            return obs_server_heartbeat(server, session, &packet->heartbeat);

        case MC_PACKET_AUTHENTICATION:
            return obs_server_authenticate(server, session, &packet->authentication);

        case MC_PACKET_HANDSHAKE:
            return obs_server_handshake(server, session, &packet->handshake);

        default:
            OBS_LOG_ERROR("server", "Received packet with ID 0x%02X, this packet is unhandled!", packet->type);
            return;
    }
}

void obs_server_process_data(struct obs_server* server, struct obs_session* session, struct obs_frame* frame) {
    // Read all packets in the buffer.
    for (size_t cursor = 0; cursor < frame->receive.bytes_in;) {
        OBS_LOG_TRACE("server", "Attempting to decode client packet");
        struct mc_proto_client_packet packet;
        int const result = mc_proto_decode_client_packet(frame->receive.buffer + cursor,
                                                         frame->receive.bytes_in - cursor,
                                                         &packet);
        if (result > 0) {
            OBS_LOG_TRACE("server", "Read %llu bytes from receive buffer on frame[%llu]", result, frame->trace);
            OBS_LOG_TRACE("server", "Dispatching packet with type ID 0x%02X on frame[%llu]",
                          packet.type, frame->trace);
            cursor += result;
            session->in.read_cursor += result;
            obs_server_dispatch_packet(server, session, &packet);
        }
        else if (result < 0 && cursor < frame->receive.bytes_in) {
            OBS_LOG_TRACE("server", "Data in receive buffer is incomplete by %llu bytes on frame[%llu]", -result,
                          frame->trace);
            // Queue up another receive, we need more data!
            obs_server_queue_recv_offset(server, session, session->socket,
                                         obs_rw_buffer_read_ptr(&session->in),
                                         obs_rw_buffer_capacity(&session->in),
                                         frame->receive.bytes_in - cursor,
                                         0);

            obs_server_release_frame(server, frame);
            return;
        }
        else {
            OBS_LOG_FATAL("server", "Received unparseable data from %08X:%d on frame[%llu], aborting!!",
                          session->address, session->port, frame->trace);
            exit(EXIT_FAILURE);
        }
    }
    OBS_LOG_TRACE("server", "All data in receive buffer is processed, queueing new recv");
    obs_server_release_frame(server, frame);
    obs_server_queue_recv(server, session, session->socket,
                          obs_rw_buffer_write_ptr(&session->in),
                          obs_rw_buffer_capacity(&session->in), 0);
}

void obs_server_handle_send(struct obs_server* server, struct obs_frame* frame, struct io_uring_cqe const* cqe) {
    struct obs_session* session = frame->session;
    struct obs_send_frame* send_frame = &frame->send;
    if (cqe->res < 0) {
        // If we get -EBADF, that just means the connection was closed. This is not an error!
        if (cqe->res != -EBADF) {
            OBS_LOG_URING_ERROR("server", "send", cqe->res);
            obs_server_queue_close(server, session, session->socket);
        }
        obs_server_release_frame(server, frame);
    }
    else {
        size_t const bytes_sent = cqe->res;
        send_frame->bytes_out += bytes_sent;
        session->total_out += bytes_sent;
        OBS_LOG_TRACE("server", "Sent %llu bytes (%llu bytes total) to %08X:%d",
                      bytes_sent, send_frame->bytes_out, session->address, session->port);
        if (send_frame->bytes_out == send_frame->buffer_size) {
            OBS_LOG_TRACE("server", "Fully sent data for frame[%llu]", frame->trace);
            obs_server_release_buffer(server, send_frame->buffer);
            obs_server_release_frame(server, frame);
        }
        else {
            // TODO: Incomplete write, queue up another one.
            free(send_frame->buffer);
            obs_server_release_frame(server, frame);
        }
    }
    obs_server_submit_queue(server);
}

/*!
 * Completes a receive operation.
 * \param server Pointer to a server structure.
 * \param frame Pointer to the packet frame.
 * \param cqe Pointer to the completion queue entry.
 */
void obs_server_handle_recv(struct obs_server* server, struct obs_frame* frame, struct io_uring_cqe const* cqe) {
    struct obs_session* session = frame->session;
    if (cqe->res < 0) {
        // If we get -EBADF, this is not really an error. Anything else is an error!
        if (cqe->res != -EBADF) {
            OBS_LOG_URING_ERROR("server", "recv", cqe->res);
            obs_server_queue_close(server, session, session->socket);
        }
        obs_server_release_frame(server, frame);
    }
    else if (cqe->res == 0) {
        OBS_LOG_INFO("server", "%08X:%d has disconnected", session->address, session->port);
        obs_server_queue_close(server, session, session->socket);
        obs_server_release_frame(server, frame);
    }
    else {
        size_t const bytes_received = cqe->res;
        // Move the write cursor ahead.
        frame->receive.bytes_in += bytes_received;
        session->in.write_cursor += bytes_received;
        session->total_in += bytes_received;
        OBS_LOG_TRACE("server", "Received %llu bytes (%llu bytes total) from %08X:%d",
                      bytes_received, frame->receive.bytes_in, session->address, session->port);
        obs_server_process_data(server, session, frame);
    }
    obs_server_submit_queue(server);
}

/*!
 * Completes an accept operation.
 * \param server Pointer to the server structure.
 * \param frame Pointer to the packet frame.
 * \param cqe Pointer to the completion queue entry.
 */
void obs_server_handle_accept(struct obs_server* server, struct obs_frame* frame, struct io_uring_cqe const* cqe) {
    if (cqe->res < 0) {
        OBS_LOG_URING_ERROR("server", "accept", cqe->res);
    }
    else {
        in_addr_t const address = ntohl(frame->accept.address.sin_addr.s_addr);
        in_port_t const port = ntohs(frame->accept.address.sin_port);
        OBS_LOG_INFO("server", "Incoming connection from %08X:%d", address, port);
        struct obs_session* session = obs_server_get_available_session(server);
        if (session == NULL) {
            OBS_LOG_WARN("server", "The server is full! Disconnecting %08X:%d", address, port);
            obs_server_queue_close(server, NULL, cqe->res);
        }
        else {
            OBS_LOG_TRACE("server", "Assigning session to connection %08X:%d", address, port);
            session->socket = cqe->res;
            session->address = address;
            session->port = port;
            session->status = SESSION_HANDSHAKING;
            session->in.ring = obs_alloc_ring_buffer(4096, 1);
            obs_server_queue_recv(server, session, session->socket,
                                  obs_rw_buffer_write_ptr(&session->in),
                                  obs_rw_buffer_capacity(&session->in), 0);
        }
    }
    // Clean up the network frame.
    obs_server_queue_accept(server, 0);
    obs_server_submit_queue(server);
    obs_server_release_frame(server, frame);
}

/*!
 * Completes a close operation.
 * \param server Pointer to the server structure.
 * \param frame Pointer to the packet frame.
 * \param cqe Pointer to the completion queue entry.
 */
void obs_server_handle_close(struct obs_server const* server, struct obs_frame* frame, struct io_uring_cqe const* cqe) {
    if (cqe->res < 0) {
        OBS_LOG_URING_ERROR("server", "close", cqe->res);
    }
    else {
        struct obs_session* session = frame->session;
        if (session != NULL) {
            OBS_LOG_INFO("server", "Server closed connection to %08X:%d", session->address, session->port);
            obs_session_release(session);
        }
        else {
            OBS_LOG_INFO("server", "Server closed connection to client");
        }
    }
    obs_server_release_frame(server, frame);
}

/*!
 * Handle a completion queue event.
 * \param server Pointer to a server structure.
 * \param cqe Pointer to a queue completion entry.
 */
void obs_server_handle_cqe(struct obs_server* server, struct io_uring_cqe const* cqe) {
    struct obs_frame* frame = (struct obs_frame*) cqe->user_data;
    OBS_LOG_TRACE("server", "Got a CQE with result %d and frame[%llu] type %s",
                  cqe->res, frame->trace, obs_frame_type_to_string(frame->type));
    switch (frame->type) {
        case OBS_FRAME_SEND:
            return obs_server_handle_send(server, frame, cqe);

        case OBS_FRAME_RECEIVE:
            return obs_server_handle_recv(server, frame, cqe);

        case OBS_FRAME_ACCEPT:
            return obs_server_handle_accept(server, frame, cqe);

        case OBS_FRAME_CLOSE:
            return obs_server_handle_close(server, frame, cqe);

        default:
            OBS_LOG_FATAL("server", "Received unknown frame[%llu] type or invalid CQE!", frame->trace);
            exit(EXIT_FAILURE);
    }
}

struct obs_server* obs_server_create(struct obs_server_params const* params) {
    OBS_LOG_TRACE("server", "Creating server structure");
    struct obs_server* server = malloc(sizeof(struct obs_server));
    OBS_LOG_TRACE("server", "Allocating %llu sessions", params->max_connections);
    server->sessions = calloc(params->max_connections, sizeof(struct obs_session));
    server->session_limit = params->max_connections;
    if (server->sessions == NULL) {
        free(server);
        return NULL;
    }
    OBS_LOG_TRACE("server", "Allocating network frame pool (%llu KB)", params->frame_pool_size / 1024);
    server->frame_allocator = obs_pool_allocator_create(sizeof(struct obs_frame), params->frame_pool_size);
    if (server->frame_allocator == NULL) {
        free(server->sessions);
        free(server);
        return NULL;
    }
    OBS_LOG_TRACE("server", "Initializing io_uring buffers (queue depth: %llu)", params->queue_depth);
    if (io_uring_queue_init(params->queue_depth, &server->ring, 0) < 0) {
        obs_pool_allocator_destroy(server->frame_allocator);
        free(server->sessions);
        free(server);
        return NULL;
    }
    return server;
}

void obs_server_destroy(struct obs_server* server) {
    io_uring_queue_exit(&server->ring);
    obs_pool_allocator_destroy(server->frame_allocator);
    free(server->sessions);
    free(server);
}

void obs_server_listen(struct obs_server* server, uint16_t const port) {
    server->socket = socket(PF_INET, SOCK_STREAM, 0);
    OBS_LOG_TRACE("server", "Acquiring socket file descriptor");
    if (server->socket < 0) {
        OBS_LOG_PERROR("server", "socket");
        return;
    }
    OBS_LOG_TRACE("server", "Acquired file descriptor %d", server->socket);
    int const enable = 1;
    if (setsockopt(server->socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
        OBS_LOG_PERROR("server", "setsockopt");
        return;
    }
    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    OBS_LOG_TRACE("server", "Binding socket");
    if (bind(server->socket, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
        OBS_LOG_PERROR("server", "bind");
        return;
    }
    OBS_LOG_TRACE("server", "Listening on socket %d", server->socket);
    if (listen(server->socket, 32) < 0) {
        OBS_LOG_PERROR("server", "listen");
        return;
    }
    obs_server_queue_accept(server, 0);
    obs_server_submit_queue(server);
}

void obs_server_close(struct obs_server* server) {
    OBS_LOG_TRACE("server", "Disconnecting connected sessions");
    for (size_t i = 0; i < server->session_limit; ++i) {
        struct obs_session const* session = &server->sessions[i];
        if (session->socket != 0) {
            OBS_LOG_TRACE("server", "Disconnecting %08X:%d", session->address, session->port);
        }
    }
    OBS_LOG_TRACE("server", "Closing server socket");
    obs_server_queue_close(server, NULL, server->socket);
    obs_server_submit_queue(server);
}

void obs_server_poll(struct obs_server* server) {
    struct io_uring_cqe* cqe;
    while (io_uring_peek_cqe(&server->ring, &cqe) == 0) {
        obs_server_handle_cqe(server, cqe);
        io_uring_cqe_seen(&server->ring, cqe);
    }
}
