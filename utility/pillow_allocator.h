#ifndef PILLOW_ALLOCATOR_H_INCLUDED
#define PILLOW_ALLOCATOR_H_INCLUDED

#include <stddef.h>
#include <inttypes.h>

typedef struct pillow_allocator {
	void* (*realloc)(void* allocator, void* ptr, size_t size, const char* file, int line);
}pillow_allocator;

#define pillow_realloc(allocator, ptr, size) allocator->realloc(allocator, ptr, size, __FILE__, __LINE__)

#define pillow_malloc(allocator, size) pillow_realloc(allocator, NULL, size)
#define pillow_free(allocator, ptr) pillow_realloc(allocator, ptr, 0)

extern pillow_allocator* pillow_heap;

#endif // !PILLOW_ALLOCATOR_H_INCLUDED
