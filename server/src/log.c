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

#include <pthread.h>
#include <stdio.h>
#include <time.h>

static char const* level_str[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL",
};

static const char* level_fg[] = {
    "\x1b[36m", "\x1b[35m", "\x1b[37m", "\x1b[33m", "\x1b[31m", "\x1b[30;41m",
};

void obs_log(int const level, char const* src, char const* fmt, ...) {
    char timestamp[20];
    time_t const t = time(NULL);
    strftime(timestamp, sizeof(timestamp), "%Y/%m/%d %H:%M:%S", localtime(&t));

    fprintf(stdout, "\x1b[2m%s\x1b[22m %s%5s\x1b[0m \x1b[1m[%10s]\x1b[0m ",
            timestamp, level_fg[level], level_str[level], src);

    va_list va;
    __builtin_va_start(va, fmt);
    vfprintf(stdout, fmt, va);
    __builtin_va_end(va);
    fprintf(stdout, "\n");

    fflush(stdout);
}
