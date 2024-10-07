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

#ifndef OBSIDIAN_SERVER_H
#define OBSIDIAN_SERVER_H

#include <stddef.h>
#include <stdint.h>

/*!
 * Parameters passed at server creation time to configure the server.
 */
struct obs_server_params {
    /// Maximum amount of connected clients.
    size_t max_connections;

    /// Queue depth of the I/O ring buffers. May be zero to let the server decide.
    unsigned queue_depth;

    /// Size of the frame pool in bytes. May be zero to let the server decide.
    size_t frame_pool_size;
};


/*!
 * Client session data.
 */
struct obs_session;


/*!
 * \brief Asynchronous server that implements the Minecraft multiplayer protocol.
 *
 * obs_sever is an implementation of the Minecraft multiplayer protocol, internally it uses io_uring to asynchronously
 * process network calls. The server does not keep track of the world at all, but instead forwards processed packets.
 */
struct obs_server;


/*!
 * Creates a new server.
 * \param params Pointer to an obs_server_params structure.
 * \return Pointer to the server structure, or NULL upon error.
 */
struct obs_server* obs_server_create(struct obs_server_params const* params);

/*!
 * Destroys a server and releases all allocated resources.
 * \param server Pointer to the server structure.
 */
void obs_server_destroy(struct obs_server* server);

/*!
 * Opens a socket and listens on the specified port.
 * \param server Pointer to the server structure.
 * \param port Port upon which to listen.
 */
void obs_server_listen(struct obs_server* server, uint16_t port);

/*!
 * Disconnects all clients and closes the server socket.
 * \param server Pointer to the server structure.
 */
void obs_server_close(struct obs_server* server);

/*!
 * Polls the server for any new connections or data and processes it.
 * \param server Pointer to the server structure.
 */
void obs_server_poll(struct obs_server* server);

#endif // !OBSIDIAN_SERVER_H
