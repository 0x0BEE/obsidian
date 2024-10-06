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

#include "obsidian/memory.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

static inline size_t nearest_multiple(size_t const size, size_t const multiple) {
    return (size + multiple - 1) / multiple * multiple;
}

struct obs_ring_buffer* obs_alloc_ring_buffer(size_t const size, size_t const count) {
    struct obs_ring_buffer* rb = malloc(sizeof(struct obs_ring_buffer));
    rb->size = nearest_multiple(size, getpagesize());
    rb->count = 0;
    rb->data = mmap(NULL, rb->size + rb->size * count, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (rb->data == MAP_FAILED) {
        free(rb);
        return NULL;
    }
    int const fd = memfd_create("obs_ring_buffer", 0);
    if (fd == -1) {
        obs_free_ring_buffer(rb);
        return NULL;
    }
    if (ftruncate(fd, size) == -1) {
        obs_free_ring_buffer(rb);
        close(fd);
        return NULL;
    }
    for (size_t offset = 0; offset < rb->size * (count + 1); offset += rb->size) {
        void const* slice = mmap(rb->data + offset, size, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, fd, 0);
        if (slice == MAP_FAILED) {
            obs_free_ring_buffer(rb);
            close(fd);
            return NULL;
        }
        ++rb->count;
    }
    --rb->count;
    close(fd);
    return rb;
}

void obs_free_ring_buffer(struct obs_ring_buffer* ring_buffer) {
    size_t const size = ring_buffer->size;
    size_t const count = ring_buffer->count;
    for (size_t offset = 0; offset < size * (count + 1); offset += size) {
        munmap(ring_buffer->data + offset, size);
    }
    munmap(ring_buffer->data, ring_buffer->size * (ring_buffer->count + 1));
    free(ring_buffer);
}
