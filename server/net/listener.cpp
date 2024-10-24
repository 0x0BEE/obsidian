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

#include "listener.hpp"

#include <memory>
#include <netdb.h>
#include <system_error>

constexpr static int enable = 1;

namespace obsidian::net {
    listener::listener(addrinfo const* address_info, int const backlog)
        : socket_(address_info->ai_family, address_info->ai_socktype, address_info->ai_protocol) {
        if (setsockopt(socket_.handle(), SOL_SOCKET, SO_REUSEADDR, &enable, sizeof enable) < 0) {
            throw std::system_error{errno, std::generic_category(), "setsockopt"};
        }

        if (bind(socket_.handle(), address_info->ai_addr, address_info->ai_addrlen) < 0) {
            throw std::system_error{errno, std::generic_category(), "bind"};
        }

        if (::listen(socket_.handle(), backlog) < 0) {
            throw std::system_error{errno, std::generic_category(), "listen"};
        }
    }

    listener listener::create(std::string const& address, std::string const& port, int const backlog) {
        addrinfo info = {};
        info.ai_family = AF_UNSPEC;
        info.ai_socktype = SOCK_STREAM;
        info.ai_flags = AI_PASSIVE;

        std::unique_ptr<addrinfo> serv_info(nullptr); {
            addrinfo* result;
            if (getaddrinfo(address.c_str(), port.c_str(), &info, &result) < 0) {
                throw std::system_error{errno, std::generic_category(), "getaddrinfo"};
            }
            serv_info.reset(result);
        }

        for (addrinfo* p = serv_info.get(); p != nullptr; p = p->ai_next) {
            try {
                return listener{p, backlog};
            }
            catch (...) {
                // TODO: Potentially log a warning.
            }
        }

        throw std::runtime_error("could not create listener");
    }
}
