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

#include <netdb.h>
#include <string>

#include "net/socket.hpp"

namespace obsidian::net {
    /*!
     * \brief Implements a TCP/IPv6 listening socket.
     */
    class listener {
        socket socket_;

        /*!
         * Initializes a listener.
         * \param address_info Info structure detailing what kind of socket and listener this is.
         * \param backlog Amount of queued up connections in the accept queue.
         */
        explicit listener(addrinfo const* address_info, int backlog = default_backlog);

    public:
        /// Default amount of connections to keep in the kernel accept queue.
        constexpr static int default_backlog = 8;

        /*!
         * Constructs a TCP listener.
         * \param address Address to bind the listener to.
         * \param port Port to bind the listener to.
         * \param backlog Amount of queued up connections to keep in the accept queue.
         * \return Listener instance.
         * \note This may create either a IPv4 or IPv6 listener, depending on the value of address. This factory
         *       function makes use of getaddrinfo() to resolve what kind of listener socket to create.
         */
        static listener create(std::string const& address, std::string const& port, int backlog = default_backlog);
    };
}
