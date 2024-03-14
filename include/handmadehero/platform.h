#ifndef HANDMADEHERO_PLATFORM_H
#define HANDMADEHERO_PLATFORM_H

#include "types.h"

#if HANDMADEHERO_INTERNAL

struct read_file_result {
  u64 size;
  void *data;
};
struct read_file_result PlatformReadEntireFile(char *path);
u8 PlatformWriteEntireFile(char *path, u64 size, void *data);
void PlatformFreeMemory(void *address);

#endif

#endif /* HANDMADEHERO_PLATFORM_H */
