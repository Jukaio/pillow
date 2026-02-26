

#include "pillow_executable.h"
#include "pillow_registry.h"
#include "pillow_array.h"
#include "pillow_allocator.h"
#include "pillow_case.h"


#include <stdlib.h>
#include <assert.h>


static void* pillow_heap_realloc(void* allocator, void* ptr, size_t size, const char* file, int line)
{
	pillow_allocator* impl = (pillow_allocator*)allocator;
	(void)(impl); // Unused

	return realloc(ptr, size);

}

pillow_allocator pillow_heap_allocator = { .realloc = pillow_heap_realloc };
pillow_allocator* pillow_heap = &pillow_heap_allocator;

// TODO: Setup dynamic and static loading for this!
#ifdef pillow_static_export
#endif


int main(int argc, char** argv)
{
	// If static, else load...
	pillow_registry_api* registry = api_pillow_registry;

	pillow_executable_state_t* state = pillow_case_executable->reload(pillow_heap, registry, NULL);

	for (;;) {
		pillow_exectuable_status_t status = pillow_case_executable->execute(state);
		if (status != pillow_executable_run) {
			break;
		}
	}

	pillow_case_executable->shutdown(pillow_heap, registry, state);

	return 0;
}