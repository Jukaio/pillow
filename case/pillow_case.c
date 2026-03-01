
#include "pillow_case.h"
#include "pillow_allocator.h"
#include "pillow_array.h"
#include "pillow_case_nuklear.h"
#include "pillow_executable.h"
#include "pillow_registry.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_clipboard.h"
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_render.h"
#include "SDL3/SDL_video.h"

#include "nuklear_default.h"
#include "nuklear_demo.h"

typedef uint32_t pillow_case_bool;

#define pillow_case_false 0
#define pillow_case_true 1

#define pillow_case_editor_container_capacity 16

struct pillow_nk_sdl_t;

typedef struct pillow_case_editor_t
{
	const char *title;
} pillow_case_editor_t;

typedef struct pillow_case_editor_container_t
{
	pillow_case_editor_t entries[pillow_case_editor_container_capacity];
	size_t count;
} pillow_case_editor_container_t;

typedef struct pillow_case_executable_t
{
	pillow_executable_state_t state;
	SDL_Window *window;
	SDL_Renderer *renderer;

	struct pillow_nk_sdl_t *nk;

	pillow_case_editor_container_t editors;
} pillow_case_executable_t;

pillow_case_bool pillow_case_editor_container_add(pillow_case_editor_container_t *set, const char *title)
{
	if (set->count == pillow_array_size(set->entries)) {
		return pillow_case_false;
	}

	pillow_case_editor_t *editor = set->entries + set->count;
	set->count = set->count + 1;
	editor->title = title;
	return pillow_case_true;
}

pillow_case_bool pillow_case_editor_container_next(pillow_case_editor_container_t *set, pillow_case_editor_t **editor)
{
	if (*editor == NULL) {
		if (set->count == 0) {
			return pillow_case_false;
		}

		*editor = set->entries;
		return pillow_case_true;
	}

	*editor = *editor + 1;
	pillow_case_editor_t *end = set->entries + set->count;
	if (*editor >= end) {
		return pillow_case_false;
	}
	return pillow_case_true;
}

static struct nk_rect pillow_case_menu_bar(pillow_case_executable_t *exe, struct nk_rect bounds)
{
	struct nk_context *ctx = nk(exe->nk);
	const float menu_bar_height = 38;

	struct nk_rect menu_bar_bounds = bounds;
	menu_bar_bounds.h = menu_bar_height;

	bounds.y = bounds.y + menu_bar_bounds.h;
	bounds.h = bounds.h - menu_bar_bounds.h;

	if (nk_begin(ctx, "Pillow Menu Bar", menu_bar_bounds, 0)) {
		nk_menubar_begin(ctx);
		nk_layout_row_static(ctx, menu_bar_height, 64, 1);
		if (nk_menu_begin_label(ctx, "Editors", NK_TEXT_LEFT, nk_vec2(200, 600))) {
			pillow_case_editor_t *editor = NULL;
			while (pillow_case_editor_container_next(&exe->editors, &editor)) {
				nk_layout_row_dynamic(ctx, 25, 1);
				if (nk_menu_item_label(ctx, editor->title, NK_TEXT_LEFT)) {
					nk_window_set_focus(ctx, editor->title);
					
					// Might be worth making this an extension - This way we can stop trickling input down!
					// It could also cause problems. Well, well, well
					memset(&ctx->input.mouse.buttons[NK_BUTTON_LEFT], 0, sizeof(*ctx->input.mouse.buttons));
				}
			}
			nk_menu_end(ctx);
		}
		
		nk_menubar_end(ctx);
	}
	nk_end(ctx);
	return bounds;
}

static void pillow_case_editors(pillow_case_executable_t *exe, struct nk_rect bounds)
{
	struct nk_context *ctx = nk(exe->nk);

	pillow_case_editor_t *editor = NULL;
	while (pillow_case_editor_container_next(&exe->editors, &editor)) {
		const enum nk_widow_flags decoration = NK_WINDOW_TITLE | NK_WINDOW_BORDER;
		const enum nk_window_flags functionality = NK_WINDOW_SCROLL_AUTO_HIDE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_CLOSABLE | NK_WINDOW_SCALABLE | NK_WINDOW_MOVABLE;
		const enum nk_window_flags flags = decoration | functionality;

		static int count = 0;
		if (nk_begin(ctx, editor->title, bounds, flags)) {
			if (nk_dock_popup(ctx, bounds.x, bounds.y, bounds.w, bounds.h)) {
				nk_layout_row_dynamic(ctx, 42.0f, 1);
				nk_button_label(ctx, "Docked");
			}
		}
		nk_end(ctx);
	}
}

static pillow_exectuable_status_t pillow_case_execute(pillow_executable_state_t *state)
{
	pillow_case_executable_t *exe = (pillow_case_executable_t *)state;

	struct nk_context *ctx = nk(exe->nk);

	nk_input_begin(ctx);
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		nk_sdl_handle_event(exe->nk, &event);
	}
	nk_input_end(ctx);

	int width, height;
	SDL_GetWindowSize(exe->window, &width, &height);
	struct nk_rect bounds = nk_rect(0, 0, width, height);

	if (nk_dock_begin(ctx, 0, 0, (float)width, (float)height)) {

		bounds = pillow_case_menu_bar(exe, bounds);
		pillow_case_editors(exe, bounds);

		nk_dock_end(ctx);
	}

	// nk_demo_overview(ctx);
	//   nk_demo_configurator(ctx);

	SDL_SetRenderDrawColor(exe->renderer, 0, 0, 0, 0);
	SDL_RenderClear(exe->renderer);

	nk_sdl_draw(exe->nk, exe->renderer);

	SDL_RenderPresent(exe->renderer);

	return state->status;
}

static pillow_executable_state_t *pillow_case_reload(pillow_allocator *allocator, pillow_registry_api *registry, pillow_executable_state_t *previous)
{
	pillow_case_executable_t exe = {0};
	exe.state.name = pillow_case;
	exe.state.status = pillow_executable_run;

	if (previous == NULL) {
		SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
		exe.window = SDL_CreateWindow("Pillow Case", 1280, 720, SDL_WINDOW_RESIZABLE /* | SDL_WINDOW_BORDERLESS */);
		exe.renderer = SDL_CreateRenderer(exe.window, NULL);
		exe.nk = nk_sdl_allocate(allocator, exe.renderer);

		pillow_case_editor_container_add(&exe.editors, "Editor 0");
		pillow_case_editor_container_add(&exe.editors, "Editor 1");
		pillow_case_editor_container_add(&exe.editors, "Editor 2");
		pillow_case_editor_container_add(&exe.editors, "Editor 3");
	}

	if (previous != NULL) {
		// Copy stuff over!

		registry->remove(previous);
	}

	pillow_case_executable_t *out = pillow_register_value(registry, pillow_executable, exe);
	return &out->state;
}

void pillow_case_shutdown(struct pillow_allocator *allocator, struct pillow_registry_api *registry, pillow_executable_state_t *state)
{
	pillow_case_executable_t *exe = (pillow_case_executable_t *)state;

	nk_sdl_free(allocator, exe->nk);

	SDL_DestroyRenderer(exe->renderer);
	SDL_DestroyWindow(exe->window);

	SDL_Quit();

	pillow_free(allocator, exe);
}

static pillow_executable_interface pillow_case_implementation = {
	.reload = pillow_case_reload,
	.shutdown = pillow_case_shutdown,
	.execute = pillow_case_execute,
};
pillow_executable_interface *pillow_case_executable = &pillow_case_implementation;