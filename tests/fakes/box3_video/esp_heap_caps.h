#pragma once
#include <stddef.h>

#define MALLOC_CAP_INTERNAL (1U << 0)
#define MALLOC_CAP_8BIT (1U << 1)
#define MALLOC_CAP_SPIRAM (1U << 2)
#define MALLOC_CAP_DMA (1U << 3)

void *heap_caps_calloc(size_t count, size_t size, unsigned caps);
void *heap_caps_malloc(size_t size, unsigned caps);
void heap_caps_free(void *pointer);
