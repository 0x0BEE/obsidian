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

#pragma once

#include "io/file_descriptor.hpp"

namespace obsidian::net {
    /*!
     * \brief Wrapper around socket(2).
     */
    class socket {
        io::file_descriptor fd_;

    public:
        /*!
         * \brief Creates an invalid socket.
         */
        socket() = default;

        /*!
         * \brief Creates a socket from a file descriptor.
         * \param fd Socket file descriptor.
         * \note This takes ownership of fd.
         */
        explicit socket(io::file_descriptor::handle_type fd);

        /*!
         * \brief Creates a socket.
         * \see ::socket()
         */
        explicit socket(int domain, int type, int protocol = 0);

        /*!
         * \brief Returns the file descriptor of this socket.
         * \return File descriptor.
         */
        [[nodiscard]]
        io::file_descriptor& file_descriptor() noexcept {
            return fd_;
        }

        /*!
         * \brief Returns the file descriptor of this socket.
         * \return File descriptor.
         */
        [[nodiscard]]
        io::file_descriptor const& file_descriptor() const noexcept {
            return fd_;
        }

        /*!
         * \brief Returns the file descriptor handle of this socket.
         * \return File descriptor handle.
         */
        [[nodiscard]]
        io::file_descriptor::handle_type handle() const noexcept {
            return fd_.handle();
        }
    };
}
