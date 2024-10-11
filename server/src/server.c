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

#include <liburing.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>


struct obs_server {
    int socket;
    struct io_uring ring;
};


int obs_server_create(struct obs_server** server_ptr) {
    struct obs_server* server = malloc(sizeof(struct obs_server));
    if (server == NULL) {
        OBS_LOG_PERROR("server", "malloc");
        return -ENOMEM;
    }

    int const result = io_uring_queue_init(128, &server->ring, 0);
    if (result < 0) {
        OBS_LOG_PERROR("server", "io_uring_queue_init");
        close(server->socket);
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
    server_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(server->socket, (struct sockaddr*) &server_address, sizeof(server_address)) < 0) {
        OBS_LOG_PERROR("server", "bind");
        return -errno;
    }
    if (listen(server->socket, 16) < 0) {
        OBS_LOG_PERROR("server", "listen");
        return -errno;
    }
    return 0;
}

void obs_server_close(struct obs_server* server) {
    close(server->socket);
    server->socket = 0;
}

int obs_server_poll(struct obs_server* server) {
    return 0;
}
