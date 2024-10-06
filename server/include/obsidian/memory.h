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

#ifndef OBSIDIAN_MEMORY_H
#define OBSIDIAN_MEMORY_H

#include <stddef.h>


/*!
 * A pool allocator that provides very fast (de-)allocations.
 *
 * Internally the pool allocator allocates a page boundary aligned memory pool that is divided up into equally sized
 * elements. Each element of the pool is initialized with a pointer to the next element in the pool. The only data
 * stored by the pool allocator itself is a pointer to the next free element.
 *
 * When making an allocation, the pool allocator takes the next free element, and copies the pointer to the next free
 * element in that element to itself. When deallocating, the element is turned into a pointer to the current next
 * free element, and the deallocated element takes the place of the next free element. This means that allocation is
 * just a single pointer assignment, and de-allocation is two pointer assignments.
 *
 * The pool does not grow or shrink, and guarantees that the allocated pool is in contiguous memory.
 */
struct obs_pool_allocator;


/*!
 * Initializes a new pool allocator.
 * \param element_size Size of each element in the pool.
 * \param size Size of the pool.
 * \return Pointer to an initialized pool allocator, or NULL if out of memory.
 */
struct obs_pool_allocator* obs_pool_allocator_create(size_t element_size, size_t size);

/*!
 * Destroys a pool allocator and clears up all associated resources.
 * \param allocator Pointer to the pool allocator.
 * \note Any data allocated by this allocator is invalidated after this call. 
 */
void obs_pool_allocator_destroy(struct obs_pool_allocator* allocator);

/*!
 * Allocates another from the pool.
 * \param allocator Pointer to the pool allocator.
 * \return A pointer to the allocated memory, or NULL if the pool is out of memory.
 * \note The allocated memory will be at least the size of the element_size parameter passed to
 *       obs_pool_allocator_create()
 */
void* obs_pool_allocator_alloc(struct obs_pool_allocator* allocator);

/*!
 * Deallocates the space previously allocated by obs_pool_allocator_alloc().
 * \param allocator Pointer to the pool allocator.
 * \param ptr Pointer to the memory to deallocate.
 * \note Calling this function with ptr that is not a pointer allocated by this allocator results in
 *       undefined behavior.
 */
void obs_pool_allocator_free(struct obs_pool_allocator* allocator, void* ptr);


/*!
 * A ring buffer.
 *
 * Provides a buffer where the address range is a multiple of the buffer. For example, a buffer with a size of 4096
 * and a count of 1 has a guarantee that <code>buffer->data[0] == buffer->data[4096]</code>. This enables write and
 * read operations to safely overflow.
 *
 * Internally the buffer makes use of the Linux kernel's ability to map a virtual address range onto the same page. As
 * a result, the size of the ring buffer will always be a multiple of the page size, and the smallest possible ring
 * buffer will always be equal to the page size.
 */
struct obs_ring_buffer {
    /// Size of the ring buffer in bytes.
    size_t size;

    /// Amount of times the address space is larger than the size.
    size_t count;

    /// Pointer to the allocated memory.
    void* data;
};


/*!
 * Allocates a new ring buffer.
 * \param size Minimum size of the ring buffer. The actual size of the buffer is written out to this parameter.
 * \param count Amount of repetitions.
 * \return Pointer to allocated memory.
 * \note The real size of the buffer will be aligned to the nearest memory page size, usually 4KB.
 * \note The count parameter indicates how many times the ring buffer loops back on itself. For example, a count of 2
 *       and a size of 4096 means that <code>buffer[0] == buffer[4096] == buffer[8192]</code>.
 */
struct obs_ring_buffer* obs_alloc_ring_buffer(size_t size, size_t count);


/*!
 * Deallocates a ring buffer.
 * \param ring_buffer Pointer to a ring buffer to deallocate.
 */
void obs_free_ring_buffer(struct obs_ring_buffer* ring_buffer);

#endif // !OBSIDIAN_MEMORY_H
