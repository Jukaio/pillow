#ifndef PILLOW_EXECUTABLE_H_INCLUDED
#define PILLOW_EXECUTABLE_H_INCLUDED

#include <stddef.h>
#include <inttypes.h>

#define pillow_executable "pillow-executable"

#define pillow_executable_run 0
#define pillow_executable_shutdown 1;
typedef size_t pillow_exectuable_status_t;

struct pillow_registry_api;
struct pillow_allocator;

typedef struct pillow_executable_state_t {
	const char* name;
	pillow_exectuable_status_t status;
}pillow_executable_state_t;

typedef struct pillow_executable_interface {
	pillow_executable_state_t* (*start)(struct pillow_allocator* allocator, struct pillow_registry_api* registry);
	void (*shutdown)(struct pillow_registry_api* registry, pillow_executable_state_t* current);

	pillow_exectuable_status_t(*execute)(pillow_executable_state_t* state);
}pillow_executable_interface;

#endif // !PILLOW_EXECUTABLE_H_INCLUDED
