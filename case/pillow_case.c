
#include "pillow_case.h"
#include "pillow_executable.h"
#include "pillow_allocator.h"
#include "pillow_registry.h"
#include "pillow_array.h"
#include "pillow_case_nuklear.h"

#include "SDL3/SDL_init.h"
#include "SDL3/SDL_video.h"
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_render.h"
#include "SDL3/SDL_clipboard.h"
#include "SDL3/SDL.h"

#include "nuklear_default.h"
#include "nuklear_demo.h"

typedef uint32_t pillow_case_bool;

#define pillow_case_false 0
#define pillow_case_true 1

#define pillow_case_editor_set_capacity 16

struct pillow_nk_sdl_t;

typedef struct pillow_case_editor_t {
	const char* title;
}pillow_case_editor_t;

typedef struct pillow_case_editor_set_t {
	pillow_case_editor_t entries[pillow_case_editor_set_capacity];
	size_t count;
}pillow_case_editor_set_t;

typedef struct pillow_case_executable_t
{
	pillow_executable_state_t state;
	SDL_Window* window;
	SDL_Renderer* renderer;

	struct pillow_nk_sdl_t* nk;

	pillow_case_editor_set_t editors;
}pillow_case_executable_t;

pillow_case_bool pillow_case_editor_set_add(pillow_case_editor_set_t* set, const char* title)
{
	if (set->count == pillow_array_size(set->entries)) {
		return pillow_case_false;
	}

	pillow_case_editor_t* editor = set->entries + set->count;
	set->count = set->count + 1;
	editor->title = title;
	return pillow_case_true;
}

pillow_case_bool pillow_case_editor_set_next(pillow_case_editor_set_t* set, pillow_case_editor_t** editor)
{
	if (*editor == NULL) {
		if (set->count == 0) {
			return pillow_case_false;
		}

		*editor = set->entries;
		return pillow_case_true;
	}

	*editor = *editor + 1;
	pillow_case_editor_t* end = set->entries + set->count;
	if (*editor >= end) {
		return pillow_case_false;
	}
	return pillow_case_true;
}

static void pillow_case_editors(pillow_case_executable_t* exe)
{
	struct nk_context* ctx = nk_sdl(exe->nk);

	int width, height;
	SDL_GetWindowSize(exe->window, &width, &height);

	pillow_case_editor_t* editor = NULL;

	struct nk_panel* grabbed = NULL;

	while (pillow_case_editor_set_next(&exe->editors, &editor)) {
		const enum nk_widow_flags decoration = NK_WINDOW_TITLE | NK_WINDOW_BORDER;
		const enum nk_window_flags functionality = NK_WINDOW_SCROLL_AUTO_HIDE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_CLOSABLE | NK_WINDOW_SCALABLE | NK_WINDOW_MOVABLE;
		const enum nk_window_flags flags = decoration | functionality;

		if (nk_begin(ctx, editor->title, nk_rect(0, 0, width, height), flags)) {
			struct nk_panel* panel = nk_window_get_panel(ctx);
			struct nk_rect title_bounds = panel->bounds;
			title_bounds.h = panel->header_height;
			title_bounds.y = title_bounds.y - title_bounds.h;
			if (nk_input_is_mouse_hovering_rect(&ctx->input, title_bounds))
			{
				const struct nk_mouse_button* lbtn = &ctx->input.mouse.buttons[NK_BUTTON_LEFT];
				if (lbtn->down)
				{
					if (grabbed == NULL)
					{
						grabbed = panel;
					}
				}

				const struct nk_mouse_button* dbtn = &ctx->input.mouse.buttons[NK_BUTTON_DOUBLE];
				if (dbtn->clicked) {
					ctx->current->bounds.x = 0.0f;
					ctx->current->bounds.y = 0.0f;
					ctx->current->bounds.w = (float)width;
					ctx->current->bounds.h = (float)height;
				}
			}
		}
		nk_end(ctx);
	}


	if (grabbed)
	{
		nk_style_push_color(ctx, &ctx->style.window.background, nk_rgba(0, 0, 0, 0));
		if (nk_begin(ctx, "Hover", nk_rect(0, 0, width, height), NK_WINDOW_NO_INPUT))
		{
			//struct nk_command_buffer*  painter = nk_window_get_canvas(ctx);
			nk_layout_row_static(ctx, 140, 200, 2);
			// Sadly need to do some manual work for all that!
			struct nk_rect bounds = nk_widget_bounds(ctx);
			if (nk_input_is_mouse_hovering_rect(&ctx->input, bounds))
			{
				SDL_Log(":(");
			}
			//nk_stroke_rect(painter, bounds, 10, 3, nk_rgb(0, 255, 255));
			if (nk_button_symbol(ctx, NK_SYMBOL_RECT_OUTLINE)) {
				SDL_Log("Go left");
			}

			if (nk_button_symbol(ctx, NK_SYMBOL_RECT_OUTLINE)) {
				SDL_Log("Go right");
			}
		}
		nk_style_pop_color(ctx);
		nk_end(ctx);
	}


}

static pillow_exectuable_status_t pillow_case_execute(pillow_executable_state_t* state)
{
	pillow_case_executable_t* exe = (pillow_case_executable_t*)state;

	struct nk_context* ctx = nk_sdl(exe->nk);

	nk_input_begin(ctx);
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		nk_sdl_handle_event(exe->nk, &event);
	}
	nk_input_end(ctx);

	pillow_case_editors(exe);

	SDL_SetRenderDrawColor(exe->renderer, 0, 0, 0, 0);
	SDL_RenderClear(exe->renderer);

	nk_sdl_draw(exe->nk, exe->renderer);

	SDL_RenderPresent(exe->renderer);

	return state->status;
}

static pillow_executable_state_t* pillow_case_reload(pillow_allocator* allocator, pillow_registry_api* registry, pillow_executable_state_t* previous)
{
	pillow_case_executable_t exe = { 0 };
	exe.state.name = pillow_case;
	exe.state.status = pillow_executable_run;

	if (previous == NULL) {
		SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
		exe.window = SDL_CreateWindow("Pillow Case", 1280, 720, SDL_WINDOW_RESIZABLE /* | SDL_WINDOW_BORDERLESS */);
		exe.renderer = SDL_CreateRenderer(exe.window, NULL);
		exe.nk = nk_sdl_allocate(allocator, exe.renderer);

		pillow_case_editor_set_add(&exe.editors, "Editor 0");
		pillow_case_editor_set_add(&exe.editors, "Editor 1");
		pillow_case_editor_set_add(&exe.editors, "Editor 2");
		pillow_case_editor_set_add(&exe.editors, "Editor 3");

	}

	if (previous != NULL) {
		// Copy stuff over!

		registry->remove(previous);
	}


	pillow_case_executable_t* out = pillow_register_value(registry, pillow_executable, exe);
	return &out->state;
}

void pillow_case_shutdown(struct pillow_allocator* allocator, struct pillow_registry_api* registry, pillow_executable_state_t* state)
{
	pillow_case_executable_t* exe = (pillow_case_executable_t*)state;

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
pillow_executable_interface* pillow_case_executable = &pillow_case_implementation;