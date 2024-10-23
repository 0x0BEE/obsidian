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

namespace obsidian::io {
    class file_descriptor {
    public:
        /// File descriptor type.
        using handle_type = int;

    private:
        /// Integer value representing an invalid file descriptor.
        static constexpr handle_type invalid_fd = -1;

        /// File descriptor handle.
        handle_type fd_{invalid_fd};

    public:
        /*!
         * \brief Creates an invalid file descriptor.
         */
        file_descriptor() = default;

        /*!
         * \brief Creates a file descriptor from a handle.
         * \param fd File descriptor.
         * \note This takes ownership of fd.
         */
        explicit file_descriptor(handle_type fd);

        /*!
         * \brief Deleted copy constructor.
         */
        file_descriptor(file_descriptor const& other) = delete;

        /*!
         * \brief Move constructs a file descriptor.
         * \param other File descriptor to move from.
         */
        file_descriptor(file_descriptor&& other) noexcept;

        /*!
         * \brief Closes the file descriptor.
         */
        ~file_descriptor();

        /*!
         * \brief Deleted copy assignment.
         */
        file_descriptor& operator=(file_descriptor const& other) = delete;

        /*!
         * \brief Move assignment.
         * \param other The file descriptor to be moved from.
         * \return This file descriptor.
         * \note This will close the file descriptor if it is open.
         */
        file_descriptor& operator=(file_descriptor&& other) noexcept;

        /*!
         * Compares two file descriptors.
         * \param other The other file descriptor.
         * \return True if these are the same file descriptor.
         */
        bool operator==(file_descriptor const& other) const {
            return fd_ == other.fd_;
        }

        /*!
         * Returns whether the file descriptor is valid or not.
         * \return true if valid, false if invalid.
         */
        [[nodiscard]] bool valid() const noexcept {
            return fd_ >= 0;
        }

        /*!
         * Returns whether the file descriptor is invalid or not.
         * \return true if valid, false if invalid.
         */
        [[nodiscard]] bool invalid() const noexcept {
            return not valid();
        }

        /*!
         * \brief Returns the file descriptor handle.
         * \return File descriptor handle.
         */
        [[nodiscard]] handle_type handle() const noexcept {
            return fd_;
        }
    };
}
