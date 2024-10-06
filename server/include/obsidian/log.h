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

#ifndef OBSIDIAN_LOG_H
#define OBSIDIAN_LOG_H

#include <string.h>


enum obs_log_level {
    OBS_LOG_LEVEL_TRACE,
    OBS_LOG_LEVEL_DEBUG,
    OBS_LOG_LEVEL_INFO,
    OBS_LOG_LEVEL_WARN,
    OBS_LOG_LEVEL_ERROR,
    OBS_LOG_LEVEL_FATAL,
};


/*!
 * Logs a trace message.
 */
#define OBS_LOG_TRACE(src, ...) obs_log(OBS_LOG_LEVEL_TRACE, src, __VA_ARGS__)

/*!
 * Logs a debug message.
 */
#define OBS_LOG_DEBUG(src, ...) obs_log(OBS_LOG_LEVEL_DEBUG, src, __VA_ARGS__)

/*!
 * Logs a message.
 */
#define OBS_LOG_INFO(src, ...)  obs_log(OBS_LOG_LEVEL_INFO, src, __VA_ARGS__)

/*!
 * Logs a warning message.
 */
#define OBS_LOG_WARN(src, ...)  obs_log(OBS_LOG_LEVEL_WARN, src, __VA_ARGS__)

/*!
 * Logs an error message.
 */
#define OBS_LOG_ERROR(src, ...) obs_log(OBS_LOG_LEVEL_ERROR, src, __VA_ARGS__)

/*!
 * Logs a fatal message.
 */
#define OBS_LOG_FATAL(src, ...) obs_log(OBS_LOG_LEVEL_FATAL, src, __VA_ARGS__)

/*!
 * Logs a libc error, or anything else using errno.
 */
#define OBS_LOG_PERROR(src, fn) obs_log(OBS_LOG_LEVEL_ERROR, src, "Call to '%s' failed: %s", fn, strerror(errno))

/*!
 * Logs an error from a io_uring operation.
 */
#define OBS_LOG_URING_ERROR(src, fn, res) obs_log(OBS_LOG_LEVEL_ERROR, src, "Call to '%s' failed: %s", fn, strerror(-res))

/*!
 * Logs a message to the log output.
 * \param level One of <code>obs_log_level</code>.
 * \param src A string to describe the source of the log message.
 * \param fmt A format string.
 * \param ... Arguments to the format string.
 */
void obs_log(int level, char const* src, char const* fmt, ...);

#endif // !OBSIDIAN_LOG_H
