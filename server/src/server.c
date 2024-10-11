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
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "obsidian/minecraft/protocol.h"

#define SESSION_LIMIT 1024


struct obs_session {
    int socket;
    in_addr_t address;
    in_port_t port;

    size_t total_in;
    size_t total_out;
};


struct obs_frame_accept {
    struct sockaddr_in address;
    socklen_t address_length;
};


struct obs_frame_close {};


struct obs_frame_send {
    void* buffer;
    size_t buffer_size;
};


struct obs_frame {
    uint8_t op;
    struct obs_session* session;


    union {
        struct obs_frame_accept accept;
        struct obs_frame_close close;
        struct obs_frame_send send;
    };
};


struct obs_server {
    int socket;
    struct obs_session* sessions;
    struct obs_pool_allocator* frame_pool;
    struct io_uring ring;
};


struct obs_session* obs_server_acquire_session(struct obs_server const* server) {
    OBS_LOG_TRACE("server", "Acquiring client session");
    for (int i = 0; i < SESSION_LIMIT; ++i) {
        struct obs_session* session = &server->sessions[i];
        if (session->socket == 0) {
            return session;
        }
    }
    return NULL;
}

void obs_server_release_session(struct obs_session* session) {
    OBS_LOG_TRACE("server", "Releasing client session");
    *session = (struct obs_session){0};
}

struct obs_frame* obs_server_acquire_frame(struct obs_server const* server, uint8_t const op) {
    OBS_LOG_TRACE("server", "Acquiring network frame with op %d", op);
    struct obs_frame* frame = obs_pool_allocator_alloc(server->frame_pool);
    frame->op = op;
    return frame;
}

void obs_server_release_frame(struct obs_server const* server, struct obs_frame* frame) {
    OBS_LOG_TRACE("server", "Releasing network frame");
    obs_pool_allocator_free(server->frame_pool, frame);
}

int obs_server_queue_submit(struct obs_server* server) {
    OBS_LOG_TRACE("server", "Submitting I/O queue to kernel");
    return io_uring_submit(&server->ring);
}

void obs_server_queue_send(struct obs_server* server, struct obs_session* session,
                           int const socket, void* buffer, size_t const buffer_size,
                           int const flags, unsigned const zc_flags, int sqe_flags) {
    struct io_uring_sqe* sqe = io_uring_get_sqe(&server->ring);
    struct obs_frame* frame = obs_server_acquire_frame(server, IORING_OP_SEND);
    frame->session = session;
    frame->send.buffer = buffer;
    frame->send.buffer_size = buffer_size;
    io_uring_prep_send_zc(sqe, socket, buffer, buffer_size, flags, zc_flags);
    io_uring_sqe_set_data(sqe, frame);
    sqe->flags = sqe_flags;
}

void obs_server_queue_multishot_accept(struct obs_server* server, int const socket) {
    struct io_uring_sqe* sqe = io_uring_get_sqe(&server->ring);
    struct obs_frame* frame = obs_server_acquire_frame(server, IORING_OP_ACCEPT);
    frame->accept.address_length = sizeof(struct sockaddr_in);
    io_uring_prep_multishot_accept(sqe, socket,
                                   (struct sockaddr*) &frame->accept.address,
                                   &frame->accept.address_length, 0);
    io_uring_sqe_set_data(sqe, frame);
}

void obs_server_queue_close(struct obs_server* server, struct obs_session* session, int const socket,
                            int const sqe_flags) {
    struct io_uring_sqe* sqe = io_uring_get_sqe(&server->ring);
    struct obs_frame* frame = obs_server_acquire_frame(server, IORING_OP_CLOSE);
    frame->session = session;
    io_uring_prep_close(sqe, socket);
    io_uring_sqe_set_data(sqe, frame);
    sqe->flags = sqe_flags;
}

int obs_server_create(struct obs_server** server_ptr) {
    struct obs_server* server = malloc(sizeof(struct obs_server));
    if (server == NULL) {
        OBS_LOG_PERROR("server", "malloc");
        return -ENOMEM;
    }
    server->sessions = calloc(SESSION_LIMIT, sizeof(struct obs_session));
    if (server->sessions == NULL) {
        OBS_LOG_ERROR("server", "Could not allocate sessions");
        free(server);
        return -ENOMEM;
    }
    server->frame_pool = obs_pool_allocator_create(sizeof(struct obs_frame), 4096 * 4);
    if (server->frame_pool == NULL) {
        OBS_LOG_ERROR("server", "Could not allocate frame pool");
        free(server->sessions);
        free(server);
        return -ENOMEM;
    }
    int const result = io_uring_queue_init(128, &server->ring, 0);
    if (result < 0) {
        OBS_LOG_PERROR("server", "io_uring_queue_init");
        obs_pool_allocator_destroy(server->frame_pool);
        free(server->sessions);
        free(server);
        return result;
    }
    *server_ptr = server;
    return 0;
}

void obs_server_destroy(struct obs_server* server) {
    io_uring_queue_exit(&server->ring);
    free(server);
}

int obs_server_listen(struct obs_server* server, uint16_t const port) {
    server->socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server->socket < 0) {
        OBS_LOG_PERROR("server", "socket");
        return -1;
    }
    int const enable = 1;
    if (setsockopt(server->socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
        OBS_LOG_PERROR("server", "setsockopt");
    }
    struct sockaddr_in server_address = {0};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(server->socket, (struct sockaddr*) &server_address, sizeof(server_address)) < 0) {
        OBS_LOG_PERROR("server", "bind");
        return -errno;
    }
    if (listen(server->socket, 16) < 0) {
        OBS_LOG_PERROR("server", "listen");
        return -errno;
    }
    obs_server_queue_multishot_accept(server, server->socket);
    obs_server_queue_submit(server);
    return 0;
}

void obs_server_close(struct obs_server* server) {
    close(server->socket);
    server->socket = 0;
}

void obs_server_disconnect(struct obs_server* server, struct obs_session* session, char const* message) {
    OBS_LOG_INFO("server", "Disconnecting %08X:%d with reason: %s", session->address, session->port, message);
    struct mc_proto_disconnect disconnect = {
        .message_length = strlen(message),
        .message = (mc_utf8_char*) message,
    };
    size_t const buffer_size = -mc_proto_encode_disconnect(NULL, 0, &disconnect);
    void* buffer = calloc(buffer_size, sizeof(uint8_t));
    mc_proto_encode_disconnect(buffer, buffer_size, &disconnect);
    obs_server_queue_send(server, session, session->socket, buffer, buffer_size, 0, 0, IOSQE_IO_LINK);
    obs_server_queue_close(server, session, session->socket, 0);
    obs_server_queue_submit(server);
}

void obs_server_handle_send(struct obs_server const* server, struct io_uring_cqe const* cqe) {
    OBS_LOG_TRACE("server", "Handling send");
    struct obs_frame* frame = io_uring_cqe_get_data(cqe);
    struct obs_session* session = frame->session;
    if (cqe->res < 0) {
        OBS_LOG_URING_ERROR("server", "io_uring_prep_send_zc", cqe->res);
        return obs_server_release_frame(server, frame);
    }
    if (cqe->flags & IORING_CQE_F_NOTIF) {
        OBS_LOG_TRACE("server", "Received CQE_F_NOTIFY, deallocating buffer");
        free(frame->send.buffer);
        return obs_server_release_frame(server, frame);
    }
    session->total_out += cqe->res;
    OBS_LOG_TRACE("server", "Sent %d bytes to %08X:%d", cqe->res, session->address, session->port);
    // If we're not seeing this flag, that means we're good to deallocate this buffer now.
    if (!(cqe->flags & IORING_CQE_F_MORE)) {
        free(frame->send.buffer);
        return obs_server_release_frame(server, frame);
    }
}

void obs_server_handle_accept(struct obs_server* server, struct io_uring_cqe const* cqe) {
    struct obs_frame const* frame = io_uring_cqe_get_data(cqe);
    struct obs_session* session = obs_server_acquire_session(server);
    session->socket = cqe->res;
    session->address = ntohl(frame->accept.address.sin_addr.s_addr);
    session->port = ntohs(frame->accept.address.sin_port);
    OBS_LOG_INFO("server", "Accepting new connection from %08X:%d", session->address, session->port);
    obs_server_disconnect(server, session, "Server is closed!");
}

void obs_server_handle_close(struct obs_server* server, struct io_uring_cqe const* cqe) {
    OBS_LOG_TRACE("server", "Handling close");
    struct obs_session* session = io_uring_cqe_get_data(cqe);
    obs_server_release_session(session);
}

int obs_server_poll(struct obs_server* server) {
    struct io_uring_cqe* cqe;
    while (io_uring_peek_cqe(&server->ring, &cqe) == 0) {
        struct obs_frame* frame = io_uring_cqe_get_data(cqe);
        switch (frame->op) {
            case IORING_OP_SEND: {
                obs_server_handle_send(server, cqe);
                break;
            }

            case IORING_OP_ACCEPT: {
                obs_server_handle_accept(server, cqe);
                break;
            }

            case IORING_OP_CLOSE: {
                obs_server_handle_close(server, cqe);
                obs_server_release_frame(server, frame);
                break;
            }

            default: {
                OBS_LOG_TRACE("server", "Got unknown frame (op %d) with result %d", frame->op, cqe->res);
                obs_server_release_frame(server, frame);
            }
        }
        io_uring_cqe_seen(&server->ring, cqe);
    }
    return 0;
}
