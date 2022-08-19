#ifndef MEMORY_WHEEL_H

#include <stdio.h>

struct AppMemory {
    uint64 size;
    void *data;
    void *free;
};

inline AppMemory *
initialize_memory(uint64 mem_size) {
    AppMemory *mem = (AppMemory *)calloc(mem_size, sizeof(char));
    if (!mem) {
        printf("Could not allocate game memory. Quitting...\n");
        exit(1);
    }
    mem->size = mem_size;
    mem->data = (void *)(mem + 1);
    mem->free = mem->data;
    return mem;
}

inline void*
get_memory(AppMemory *mem, uint64 size) {
    if (size > mem->size - ((char *)mem->free - (char *)mem)) {
        printf("Not enough memory.");
        exit(1);
    }
    void *result = mem->free;
    mem->free = (char *)mem->free + size;
    return result;
}

#define MEMORY_WHEEL_H
#endif
