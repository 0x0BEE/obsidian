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

#include "net/socket.hpp"

#include <system_error>
#include <unistd.h>
#include <sys/socket.h>

namespace obsidian::net {
    socket::socket(io::file_descriptor::handle_type const fd) : fd_(fd) {
        // Intentionally left blank.
    }

    socket::socket(int const domain, int const type, int const protocol)
        : fd_(::socket(domain, type, protocol)) {
        if (fd_.invalid()) {
            throw std::system_error{errno, std::generic_category(), "socket"};
        }
    }
}
