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

#include <stdint.h>

struct obs_session;

struct obs_server;

int obs_server_create(struct obs_server** server_ptr);

void obs_server_destroy(struct obs_server* server);

int obs_server_listen(struct obs_server* server, uint16_t port);

void obs_server_close(struct obs_server* server);

int obs_server_poll(struct obs_server* server);

void obs_server_disconnect(struct obs_server* server, struct obs_session* session, char const* message);

#endif // !OBSIDIAN_SERVER_H
