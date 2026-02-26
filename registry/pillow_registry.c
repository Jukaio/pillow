
#include "pillow_registry.h"
#include "pillow_array.h"

#include <string.h>

typedef struct pillow_registry_object_entry_t
{
	uint64_t aligned_memory[pillow_registry_byte_capacity_per_entry / sizeof(uint64_t)];

	const char* name;
	uint64_t hash;
} pillow_registry_object_entry_t;

typedef struct pillow_registry_t
{
	pillow_registry_object_entry_t entries[pillow_registry_byte_capacity];
	size_t count;
} pillow_registry_t;

static pillow_registry_t pillow_registry_object = { 0 };

static uint64_t pillow_registry_hash(const char* str) {
	uint64_t hash = 5381;
	int c;

	while ((c = *str++)) {
		// hash * 33 + c
		hash = (hash * 33) + c;
	}
	return hash;
}

void* pillow_registry_push(const char* name, void* registerable, size_t size)
{
	if (size > sizeof(*pillow_registry_object.entries)) {
		return NULL;
	}

	if (pillow_registry_object.count + 1 == pillow_array_size(pillow_registry_object.entries)) {
		return NULL;
	}

	uint64_t hash = pillow_registry_hash(name);
	uint64_t at = hash % pillow_registry_byte_capacity;

	for (;;) {
		pillow_registry_object_entry_t* entry = pillow_registry_object.entries + at;
		if (entry->name == NULL || entry->hash == UINT64_MAX)
		{
			// No clue if we should memcpy/strdup the name... No need for now
			entry->name = name;
			entry->hash = hash;
			memcpy(entry->aligned_memory, registerable, size);
			pillow_registry_object.count = pillow_registry_object.count + 1;
			return (void*)entry->aligned_memory;
		}
		at = (at + 1) % pillow_registry_byte_capacity;
	}
}

pillow_registry_bool_t pillow_registry_pop(const char* name)
{
	uint64_t hash = pillow_registry_hash(name);
	uint64_t at = hash % pillow_registry_byte_capacity;

	for (;;) {
		pillow_registry_object_entry_t* entry = pillow_registry_object.entries + at;
		if (entry->name != NULL)
		{
			entry->name = NULL;
			entry->hash = INT64_MAX;
			memset(entry->aligned_memory, 0, sizeof(entry->aligned_memory));
			pillow_registry_object.count = pillow_registry_object.count - 1;
			return pillow_registry_true;
		}

		if (pillow_registry_object.count == 0 || entry->hash == 0) {
			return pillow_registry_false;
		}
		at = (at + 1) % pillow_registry_byte_capacity;
	}
}

pillow_registry_bool_t pillow_registry_remove(void* registrable)
{
	const size_t self_memory_end_offset = sizeof(pillow_registry_object.entries);

	uint8_t* self_memory = (uint8_t*)pillow_registry_object.entries;
	uint8_t* incoming_memory = (uint8_t*)registrable;

	if (incoming_memory < self_memory || incoming_memory > self_memory + self_memory_end_offset)
	{
		return pillow_registry_false;
	}

	pillow_registry_object_entry_t* entry = (pillow_registry_object_entry_t*)registrable;
	if (entry->name == NULL || entry->hash == 0 || entry->hash == UINT64_MAX) {
		return pillow_registry_false;
	}

	entry->name = NULL;
	entry->hash = INT64_MAX;
	memset(entry->aligned_memory, 0, sizeof(entry->aligned_memory));

	return pillow_registry_true;
}

void* pillow_registry_get(const char* name, pillow_registry_entry_reference* result)
{
	uint64_t hash = pillow_registry_hash(name);
	uint64_t at = hash % pillow_registry_byte_capacity;

	if (pillow_registry_object.count == 0) {
		return NULL;
	}

	for (;;) {
		pillow_registry_object_entry_t* entry = pillow_registry_object.entries + at;
		if (entry->hash == hash)
		{
			if (strcmp(entry->name, name) == 0) {
				if (result != NULL) {
					result->handle = (pillow_registry_entry_handle_t)entry->aligned_memory;
				}
				return (void*)entry->aligned_memory;
			}
		}
		if (entry->hash == 0) {
			return NULL;
		}
		at = (at + 1) % pillow_registry_byte_capacity;
	}
}

void* pillow_registry_next(pillow_registry_entry_reference* result)
{
	if (result->handle == NULL) {
		return NULL;
	}

	pillow_registry_object_entry_t* begin = (pillow_registry_object_entry_t*)pillow_registry_object.entries;
	pillow_registry_object_entry_t* entry = (pillow_registry_object_entry_t*)result->handle;

	uint64_t hash = entry->hash;
	uint64_t at = ((uint64_t)(entry - begin) + 1) % pillow_registry_byte_capacity;
	const char* name = entry->name;

	for (;;) {
		pillow_registry_object_entry_t* entry = pillow_registry_object.entries + at;
		if (entry->hash == hash)
		{
			if (strcmp(entry->name, name) == 0) {
				result->handle = (pillow_registry_entry_handle_t)entry->aligned_memory;

				return (void*)entry->aligned_memory;
			}
		}
		if (entry->hash == 0) {
			return NULL;
		}
		at = (at + 1) % pillow_registry_byte_capacity;
	}
}

static pillow_registry_api pillow_registry_implementation = {
	.get = pillow_registry_get,
	.next = pillow_registry_next,
	.pop = pillow_registry_pop,
	.push = pillow_registry_push,
	.remove = pillow_registry_remove,
};

pillow_registry_api* api_pillow_registry = &pillow_registry_implementation;