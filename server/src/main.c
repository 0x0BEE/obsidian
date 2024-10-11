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

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include "obsidian/server.h"

int main() {
    struct obs_server* server;
    if (obs_server_create(&server) < 0) {
        return EXIT_FAILURE;
    }
    if (obs_server_listen(server, 25565) < 0) {
        return EXIT_FAILURE;
    }
    while (true) {
        obs_server_poll(server);
        clock_nanosleep(CLOCK_MONOTONIC, 0, &(struct timespec){
                            .tv_sec = 0, .tv_nsec = 1000000
                        }, NULL);
    }
    return EXIT_SUCCESS;
}
