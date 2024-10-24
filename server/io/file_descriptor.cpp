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

#include "io/file_descriptor.hpp"

#include <unistd.h>
#include <utility>

namespace obsidian::io {
    file_descriptor::file_descriptor(handle_type const fd) noexcept
        : fd_(fd) {
        // Intentionally left blank.
    }

    file_descriptor::file_descriptor(file_descriptor&& other) noexcept
        : fd_(std::exchange(other.fd_, invalid_fd)) {
        // Intentionally left blank.
    }

    file_descriptor::~file_descriptor() noexcept {
        close(fd_);
    }

    file_descriptor& file_descriptor::operator=(file_descriptor&& other) noexcept {
        if (fd_ != other.fd_) {
            close(fd_);
        }
        fd_ = std::exchange(other.fd_, invalid_fd);
        return *this;
    }
}
