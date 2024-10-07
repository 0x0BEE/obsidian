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

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

static inline size_t nearest_multiple(size_t const size, size_t const multiple) {
    return (size + multiple - 1) / multiple * multiple;
}


struct obs_pool_element {
    struct obs_pool_element* next;
};


struct obs_pool_allocator {
    struct obs_pool_element* next;
    uint8_t pool[];
};


struct obs_pool_allocator* obs_pool_allocator_create(size_t const element_size, size_t const size) {
    // Widen the pool size to the nearest multiple of the page size.
    size_t const page_sz = getpagesize();
    size_t const pool_size = nearest_multiple(size, page_sz);
    size_t const element_count = pool_size / element_size;
    struct obs_pool_allocator* allocator = aligned_alloc(page_sz, sizeof(struct obs_pool_allocator) + pool_size);
    if (allocator == NULL) {
        return NULL; // Out of memory!
    }
    // The first allocation is at the start of the pool.
    allocator->next = (struct obs_pool_element*) &allocator->pool[0];
    struct obs_pool_element* prev = allocator->next;
    for (size_t offset = 0; offset < pool_size; offset += element_size) {
        struct obs_pool_element* element = (struct obs_pool_element*) (allocator->pool + offset);
        prev->next = element;
        prev = element;
    }
    prev->next = NULL;
    return allocator;
}

void obs_pool_allocator_destroy(struct obs_pool_allocator* allocator) {
    free(allocator);
}

void* obs_pool_allocator_alloc(struct obs_pool_allocator* allocator) {
    struct obs_pool_element* element = allocator->next;
    allocator->next = element->next;
    return element;
}

void obs_pool_allocator_free(struct obs_pool_allocator* allocator, void* ptr) {
    struct obs_pool_element* element = ptr;
    element->next = allocator->next;
    allocator->next = element;
}
