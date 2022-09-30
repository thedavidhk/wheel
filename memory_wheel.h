#ifndef MEMORY_WHEEL_H

#include <stdio.h>
#include <string.h>

struct AppMemory {
    uint64 total_size;
    uint64 used_size;
    uint64 fragmented;
    void *data;
    void *free;
};

/* A block of free memory of arbitrary size.
 *
 * The beginning of every writable block of memory gets initialized to a
 * FreeMemoryBlock.
 */
struct FreeMemoryBlock {
    uint64 size;
    FreeMemoryBlock *next;
};

/* Initialize memory.
 *
 * The system allocates mem_size bytes of memory, sets all bytes to zero and
 * the beginning of the memory block gets initialized to a FreeMemoryBlock.
 */
inline AppMemory *
initialize_memory(uint64 mem_size) {
    AppMemory *mem = (AppMemory *)calloc(mem_size, sizeof(char));
    if (!mem) {
        printf("Could not allocate game memory. Quitting...\n");
        exit(1);
    }
    memset(mem, 0, mem_size);
    mem->total_size = mem_size;
    mem->used_size = 0;
    mem->data = (void *)(mem + 1);
    mem->free = mem->data;
    FreeMemoryBlock *block = ((FreeMemoryBlock *)mem->free);
    block->size = mem_size;
    block->next = 0;
    return mem;
}

/* Free 'size' bytes of memory in '*mem'.
 *
 * The location at 'ptr' gets initialized to a free memory block of size 'size'
 * that points to the previously most recent FreeMemoryBlock.
 *
 * TODO: Join adjacent memory blocks.
 */
inline void
free_memory(AppMemory *mem, void *ptr, uint64 size) {
    FreeMemoryBlock *new_free = (FreeMemoryBlock *) ptr;
    new_free->size = size;
    new_free->next = (FreeMemoryBlock *)mem->free;
    mem->free = new_free;
    mem->used_size -= size;
    if (!(((FreeMemoryBlock *)(mem->free))->next)) {
        mem->fragmented -= size;
    }
}

/* Get 'size' bytes of memory from '*mem'.
 * 
 * Loop over free memory blocks to find the first one that has the sufficient
 * size.
 */
inline void*
get_memory(AppMemory *mem, uint64 size) {
    FreeMemoryBlock *first = (FreeMemoryBlock *)mem->free;
    FreeMemoryBlock *candidate = first;
    FreeMemoryBlock *previous = first;
    while (candidate->size < size) {
        if (candidate->next) {
            previous = candidate;
            candidate = candidate->next;
        }
        else {
            printf("Insufficient memory. Quitting...\n");
            exit(1);
        }
    }
    mem->used_size += size;
    mem->fragmented += size;
    FreeMemoryBlock *new_free;
    if (candidate->size - size >= sizeof(FreeMemoryBlock)) {
        new_free = (FreeMemoryBlock *)((char *)candidate + size);
        new_free->next = candidate->next;
        new_free->size = candidate->size - size;
    } else {
        new_free = candidate->next;
    }
    if (candidate == first) {
        mem->free = new_free;
    }
    else {
        previous->next = new_free;
        ;
    }
    return (void *)candidate;
}

#define MEMORY_WHEEL_H
#endif
