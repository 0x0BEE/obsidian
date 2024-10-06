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

#include <liburing.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>


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

    /// Remote address of the connecting client.
    uint32_t address;

    /// Port of the connecting client.
    uint16_t port;
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
static char const* obs_frame_type_to_string(int const type) {
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

    /// Pointer to the connection session.
    struct obs_session* session;


    /// Union of possible frame data types. See type for how to interpret this data.
    union {
        struct obs_accept_frame accept;
    };
};


/*!
 * Allocates a new packet frame.
 * \param server Pointer to a server structure.
 * \param session Pointer to a client session. May be NULL.
 * \param type One of obs_frame_type.
 * \return Pointer to the allocated packet frame.
 */
struct obs_frame* obs_frame_create(struct obs_server const* server, struct obs_session* session, int const type) {
    OBS_LOG_TRACE("server", "Creating new %s packet frame", obs_frame_type_to_string(type));
    struct obs_frame* frame = obs_pool_allocator_alloc(server->frame_allocator);
    frame->type = type;
    frame->session = session;
    return frame;
}

/*!
 * Allocates a new ACCEPT packet frame.
 * \param server Pointer to a server structure.
 * \param session Pointer to a client session. May be NULL.
 * \return Pointer to the allocated packet frame.
 */
struct obs_frame* obs_frame_create_accept(struct obs_server const* server, struct obs_session* session) {
    struct obs_frame* frame = obs_frame_create(server, session, OBS_FRAME_ACCEPT);
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
    struct obs_frame* frame = obs_frame_create(server, session, OBS_FRAME_CLOSE);
    return frame;
}

/*!
 * Submits enqueued I/O operations to the kernel.
 * \param server Pointer to a server structure.
 * \return The result of io_uring_submit().
 * \see io_uring_submit()
 */
int obs_server_submit_queue(struct obs_server* server) {
    OBS_LOG_TRACE("server", "Submitting I/O queue");
    return io_uring_submit(&server->ring);
}

/*!
 * Queues an accept operation to the I/O ring buffer.
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
 * Queues a close operation to the I/O ring buffer.
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
            session->socket = cqe->res;
            session->address = address;
            session->port = port;
            obs_server_queue_close(server, session, cqe->res);
        }
    }
    // Clean up the network frame.
    obs_server_queue_accept(server, 0);
    obs_server_submit_queue(server);
    obs_pool_allocator_free(server->frame_allocator, frame);
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
            OBS_LOG_TRACE("server", "Releasing session of %08X:%d", session->address, session->port);
            *session = (struct obs_session){0};
        }
        else {
            OBS_LOG_INFO("server", "Server closed connection to client");
        }
    }
    // Clean up the network frame.
    obs_pool_allocator_free(server->frame_allocator, frame);
}

void obs_server_handle_cqe(struct obs_server* server, struct io_uring_cqe const* cqe) {
    struct obs_frame* frame = (struct obs_frame*) cqe->user_data;
    OBS_LOG_TRACE("server", "Got a CQE with result %d and frame type %s",
                  cqe->res, obs_frame_type_to_string(frame->type));
    switch (frame->type) {
        case OBS_FRAME_ACCEPT:
            return obs_server_handle_accept(server, frame, cqe);

        case OBS_FRAME_CLOSE:
            return obs_server_handle_close(server, frame, cqe);

        default:
            OBS_LOG_FATAL("server", "Received unknown frame/CQE!");
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
