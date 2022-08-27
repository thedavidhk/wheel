#include "files_wheel.h"

Texture
load_bmp_file(const char* filename, AppMemory *mem) {
    Texture f;
    FILE *ptr;
    ptr = fopen(filename, "rb");
    if (!ptr) {
        printf("File %s could not be opened.\n", filename);
        exit(1);
    }
    uint8 header[26];
    if (!fread(header, 1, 26, ptr)) {
        printf("File %s could not be read.\n", filename);
        exit(1);
    }
    // TODO: More careful compatibility checking
    uint32 fsize = *(uint32 *)(header + 2);
    uint32 offset = *(uint32 *)(header + 10);
    f.width = *(uint32 *)(header + 18);
    f.height = *(uint32 *)(header + 22);
    f.bytes = (uint8 *)get_memory(mem, (fsize - offset));
    fseek(ptr, offset, SEEK_SET);
    for (uint32 y = f.height; y > 0; y--) {
        fread(f.pixels + (y - 1) * f.width, 4, f.width, ptr);
    }
    fclose(ptr);
    return f;
};
