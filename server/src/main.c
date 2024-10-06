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

#include "obsidian/log.h"
#include "obsidian/server.h"

#include <assert.h>
#include <stdlib.h>
#include <time.h>

int main() {
    struct obs_server* server = obs_server_create(&(struct obs_server_params){
        .queue_depth = 32,
        .max_connections = 1024,
        .frame_pool_size = 2048 * 32,
    });
    obs_server_listen(server, 25565);
    OBS_LOG_INFO("server", "Listening on port %d", 25565);
    while (1) {
        obs_server_poll(server);
        clock_nanosleep(CLOCK_MONOTONIC, 0, &(struct timespec){
                            .tv_nsec = 100000,
                        }, NULL);
    }
    obs_server_close(server);
    obs_server_destroy(server);
    return EXIT_SUCCESS;
}
