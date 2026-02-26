#ifndef PILLOW_REGISTRY_H_INCLUDED
#define PILLOW_REGISTRY_H_INCLUDED

#include <stddef.h>
#include <inttypes.h>

struct pillow_allocator;

#define pillow_registry "pillow-registry"

#define pillow_registry_byte_capacity_per_entry 512
#define pillow_registry_byte_capacity 512

#define pillow_registry_false 0
#define pillow_registry_true 1
typedef uint32_t pillow_registry_bool_t;

typedef void* pillow_registry_entry_handle_t;

typedef struct pillow_registry_entry_reference {
	pillow_registry_entry_handle_t handle;
}pillow_registry_entry_reference;

typedef struct pillow_registry_api {
	void*(*push)(const char* name, void* registerable, size_t size);
	pillow_registry_bool_t(*pop)(const char* name);
	pillow_registry_bool_t(*remove)(void* registrable);

	// Not ergonomic :( 
	void* (*get)(const char* name, pillow_registry_entry_reference* result);
	void* (*next)(pillow_registry_entry_reference* result);
}pillow_registry_api;

#define pillow_register_value(registry, name, value) registry->push(name, &value, sizeof(value))

#ifdef pillow_static_export
extern pillow_registry_api* api_pillow_registry;
#endif // pillow_static_export

#endif // !PILLOW_REGISTRY_H_INCLUDED