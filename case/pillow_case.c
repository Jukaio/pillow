
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
#define pillow_case_event_queue_capacity 32

#define pillow_case_ptr_add(t, p, i) ((t *)((void *)((uint8_t *)(p) + (i))))

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

typedef struct pillow_case_event_list_t
{
	SDL_Event entries[pillow_case_event_queue_capacity];
	size_t count;
} pillow_case_event_list_t;

typedef struct pillow_case_presenter_t
{
	// Common
	const char *title;

	// List
	// struct pillow_case_child_t *first;
	// struct pillow_case_child_t *last;
	struct pillow_case_child_t *children;

	// The window is the driver and the key!!
	// But also... This is state only relevant in host mode!!
	SDL_Window *window;
	struct pillow_nk_sdl_t *nk;
	SDL_Renderer *renderer;
	pillow_case_event_list_t events;
} pillow_case_presenter_t;

typedef struct pillow_case_presenter_container_t
{
	struct pillow_case_child_container_t *children;

	// TODO: Recycle presenter resources!

	size_t capacity;
	size_t count;
	pillow_case_presenter_t entries[1];
} pillow_case_presenter_container_t;

typedef struct pillow_case_child_t
{
	// Common
	const char *title;
	struct pillow_case_child_t *next;
	pillow_case_presenter_t *parent;
	struct nk_recti bounds;
} pillow_case_child_t;

typedef struct pillow_case_child_container_t
{
	size_t capacity;
	size_t count;
	pillow_case_child_t entries[1];
} pillow_case_child_container_t;

typedef struct pillow_case_executable_t
{
	pillow_executable_state_t state;
	pillow_allocator *allocator;

	pillow_case_child_container_t *children;
	pillow_case_presenter_container_t *presenters;
	pillow_case_editor_container_t editors;
} pillow_case_executable_t;

static int pillow_case_presenter_should_combine(const pillow_case_presenter_t *lhs, const pillow_case_presenter_t *rhs, struct nk_recti *out)
{
	int lhs_grabbed = SDL_GetWindowMouseGrab(rhs);
	int rhs_grabbed = SDL_GetWindowMouseGrab(lhs);
	if (lhs_grabbed || rhs_grabbed) {
		return 0;
	}
	SDL_Rect lhs_bounds;
	SDL_Rect rhs_bounds;
	SDL_GetWindowPosition(lhs->window, &lhs_bounds.x, &lhs_bounds.y);
	SDL_GetWindowPosition(rhs->window, &rhs_bounds.x, &rhs_bounds.y);
	SDL_GetWindowSize(lhs->window, &lhs_bounds.w, &lhs_bounds.h);
	SDL_GetWindowSize(rhs->window, &rhs_bounds.w, &rhs_bounds.h);

	if (SDL_PointInRect(&(SDL_Point){rhs_bounds.x, rhs_bounds.y}, &lhs_bounds) &&
	    SDL_PointInRect(&(SDL_Point){rhs_bounds.x + rhs_bounds.w, rhs_bounds.y + rhs_bounds.h}, &lhs_bounds)) {
		out->x = rhs_bounds.x - lhs_bounds.x;
		out->y = rhs_bounds.y - lhs_bounds.y;
		out->w = (short)rhs_bounds.w;
		out->h = (short)rhs_bounds.h;
		return 1;
	}
	return 0;
}

static int pillow_case_presenter_should_detach(const pillow_case_child_t *child, SDL_Rect *out)
{
	SDL_Rect parent_bounds;
	SDL_assert(SDL_GetWindowSize(child->parent->window, &parent_bounds.w, &parent_bounds.h));

	// The bounds of a child are local to the parent!
	if (child->bounds.x >= 0 && child->bounds.y >= 0) {
		if (child->bounds.x + child->bounds.w <= parent_bounds.w && child->bounds.y + child->bounds.h <= parent_bounds.h) {
			return 0;
		}
	}

	SDL_GetWindowPosition(child->parent->window, &parent_bounds.x, &parent_bounds.y);
	out->x = child->bounds.x + parent_bounds.x;
	out->y = child->bounds.y + parent_bounds.y;
	out->w = child->bounds.w;
	out->h = child->bounds.h;

	return 1;
}

static void pillow_case_draw(pillow_case_presenter_t *presenter, struct nk_rect bounds)
{
	struct nk_context *ctx = nk(presenter->nk);

	const enum nk_widow_flags decoration = NK_WINDOW_TITLE | NK_WINDOW_BORDER;
	const enum nk_window_flags functionality = NK_WINDOW_SCROLL_AUTO_HIDE | NK_WINDOW_BACKGROUND | NK_WINDOW_CLOSABLE;
	const enum nk_window_flags flags = decoration | functionality;
	if (nk_begin(ctx, presenter->title, bounds, flags)) {
	}
	nk_end(ctx);

	// TODO: Bridge Nk and SDL_Window

	// bounds = pillow_case_menu_bar(exe, bounds);
	/*if(nk_dock_begin(ctx)) {
	    pillow_case_editors(exe, bounds);
	    nk_dock_end(ctx);
	}*/

	// nk_demo_overview(ctx);
	// nk_demo_configurator(ctx);

	SDL_SetRenderDrawColor(presenter->renderer, 255, 0, 0, 255);
	SDL_RenderClear(presenter->renderer);

	nk_sdl_draw(presenter->nk, presenter->renderer);

	SDL_RenderPresent(presenter->renderer);
}

static SDL_HitTestResult pillow_case_hit_test(SDL_Window *win, const SDL_Point *area, void *data)
{
	pillow_case_presenter_t *presenter = (pillow_case_presenter_t *)data;

	int w, h;
	SDL_GetWindowSize(win, &w, &h);

	const int DRAG_HEIGHT = 30;
	const int RESIZE_BORDER = 8;
	const int BUTTON_ZONE = 30;
	if (area->y < RESIZE_BORDER) {
		if (area->x < RESIZE_BORDER) {
			return SDL_HITTEST_RESIZE_TOPLEFT;
		}
		if (area->x > w - RESIZE_BORDER) {
			return SDL_HITTEST_RESIZE_TOPRIGHT;
		}
		return SDL_HITTEST_RESIZE_TOP;
	}
	if (area->y > h - RESIZE_BORDER) {
		if (area->x < RESIZE_BORDER) {
			return SDL_HITTEST_RESIZE_BOTTOMLEFT;
		}
		if (area->x > w - RESIZE_BORDER) {
			return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
		}
		return SDL_HITTEST_RESIZE_BOTTOM;
	}
	if (area->x < RESIZE_BORDER) {
		return SDL_HITTEST_RESIZE_LEFT;
	}
	if (area->x > w - RESIZE_BORDER) {
		return SDL_HITTEST_RESIZE_RIGHT;
	}
	if (area->y < DRAG_HEIGHT && area->x < (w - BUTTON_ZONE)) {
		return SDL_HITTEST_DRAGGABLE;
	}

	return SDL_HITTEST_NORMAL;
}

static int pillow_case_presenter_used(pillow_case_presenter_t *presenter)
{
	if (presenter->window != NULL) {
		SDL_assert(presenter->renderer != NULL && presenter->nk != NULL);
	}
	return presenter->window != NULL && presenter->renderer != NULL && presenter->nk != NULL;
}

static size_t pillow_case_presenter_container_find_free(pillow_case_presenter_container_t *set)
{
	for (size_t index = 0; index < set->count; index++) {
		pillow_case_presenter_t *presenter = set->entries + index;
		if (!pillow_case_presenter_used(presenter)) {
			return index + 1;
		}
	}
	if (set->count == set->capacity) {
		return 0;
	}
	set->count = set->count + 1;
	return set->count;
}

static pillow_case_presenter_t *pillow_case_presenter_container_add(pillow_case_presenter_container_t *set, pillow_allocator *allocator, const char *title)
{
	size_t found = pillow_case_presenter_container_find_free(set);
	if (found) {
		pillow_case_presenter_t *entry = set->entries + found - 1;

		pillow_case_presenter_t target = {0};
		target.title = title;
		target.window = SDL_CreateWindow("Pillow Case", 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS);
		target.renderer = SDL_CreateRenderer(target.window, "opengl");
		target.nk = nk_sdl_allocate(allocator, target.renderer);

		*entry = target;

		SDL_SetWindowHitTest(entry->window, pillow_case_hit_test, &entry);
		SDL_SetRenderVSync(entry->renderer, 1);

		return entry;
	}
	return NULL;
}

static pillow_case_presenter_t *pillow_case_presenter_container_find(pillow_case_presenter_container_t *set, SDL_Window *window)
{
	for (size_t index = 0; index < set->count; index++) {
		pillow_case_presenter_t *presenter = set->entries + index;
		if (presenter->window == window) {
			return presenter;
		}
	}
	return NULL;
}

static int pillow_case_presenter_container_remove(pillow_case_presenter_container_t *set, pillow_allocator *allocator, SDL_Window *window)
{
	pillow_case_presenter_t *presenter = pillow_case_presenter_container_find(set, window);
	if (presenter) {
		nk_sdl_free(allocator, presenter->nk);
		SDL_DestroyRenderer(presenter->renderer);
		SDL_DestroyWindow(presenter->window);
		presenter->window = NULL;

		memset(presenter, 0, sizeof(*presenter));
		return 1;
	}
	return 0;
}

static void pillow_case_presenter_container_clear(pillow_allocator *allocator, pillow_case_presenter_container_t *set)
{
	for (size_t index = 0; index < set->count; index++) {
		pillow_case_presenter_t *presenter = set->entries + index;
		nk_sdl_free(allocator, presenter->nk);
		SDL_DestroyRenderer(presenter->renderer);
		SDL_DestroyWindow(presenter->window);
	}
}

static size_t pillow_case_presenter_container_push_event(pillow_case_presenter_container_t *set, const SDL_Event *event)
{
	SDL_Window *window = SDL_GetWindowFromEvent(event);
	pillow_case_presenter_t *presenter = pillow_case_presenter_container_find(set, window);
	if (presenter != NULL) {
		if (presenter->events.count == pillow_array_size(presenter->events.entries)) {
			return 0;
		}
		presenter->events.entries[presenter->events.count] = *event;
		presenter->events.count = presenter->events.count + 1;
		return presenter->events.count;
	}
	return 0;
}

static void pillow_case_event_list_clear(pillow_case_event_list_t *events)
{
	events->count = 0;
}

static pillow_case_child_container_t *pillow_case_child_container_allocate(pillow_allocator *allocator, size_t capacity)
{
	size_t byte_capacity = offsetof(pillow_case_presenter_container_t, entries[capacity]);
	pillow_case_presenter_container_t *result = (pillow_case_presenter_container_t *)pillow_malloc(allocator, byte_capacity);

	// Everything ZERO
	memset(result, 0, byte_capacity);
	result->capacity = capacity;

	return result;
}

static void pillow_case_child_container_free(pillow_allocator *allocator, pillow_case_child_container_t *set)
{
	pillow_free(allocator, set);
}

static int pillow_case_child_used(pillow_case_child_t *child)
{
	return child->parent != NULL;
}

static size_t pillow_case_child_container_find_free(pillow_case_child_container_t *set)
{
	for (size_t index = 0; index < set->count; index++) {
		pillow_case_child_t *child = set->entries + index;
		if (!pillow_case_child_used(child)) {
			return index + 1;
		}
	}
	if (set->count == set->capacity) {
		return 0;
	}
	set->count = set->count + 1;
	return set->count;
}

static pillow_case_presenter_container_t *pillow_case_presenter_allocate(pillow_allocator *allocator, size_t capacity)
{
	size_t byte_capacity = offsetof(pillow_case_presenter_container_t, entries[capacity]);
	pillow_case_presenter_container_t *result = (pillow_case_presenter_container_t *)pillow_malloc(allocator, byte_capacity);

	// Everything ZERO
	memset(result, 0, byte_capacity);
	result->capacity = capacity;

	result->children = pillow_case_child_container_allocate(allocator, capacity);

	return result;
}

static void pillow_case_presenter_free(pillow_allocator *allocator, pillow_case_presenter_container_t *set)
{
	pillow_case_presenter_container_clear(allocator, set);
	pillow_case_child_container_free(allocator, set->children);
	pillow_free(allocator, set);
}

static void pillow_case_presenter_process_events(pillow_case_presenter_container_t *set)
{
	for (size_t index = 0; index < set->count; index++) {
		pillow_case_presenter_t *presenter = set->entries + index;
		if (pillow_case_presenter_used(presenter)) {
			struct nk_context *ctx = nk(presenter->nk);
			nk_input_begin(ctx);
			for (size_t event_index = 0; event_index < presenter->events.count; event_index++) {
				union SDL_Event *event = presenter->events.entries + event_index;
				nk_sdl_handle_event(presenter->nk, event);
			}
			nk_input_end(ctx);
			pillow_case_event_list_clear(&presenter->events);
		}
	}
}

static void pillow_case_presenter_container_draw(pillow_case_presenter_container_t *set)
{
	for (size_t index = 0; index < set->count; index++) {
		pillow_case_presenter_t *presenter = set->entries + index;
		if (pillow_case_presenter_used(presenter)) {
			struct nk_context *ctx = nk(presenter->nk);
			pillow_case_child_t *child = presenter->children;
			while (child) {
				struct nk_rect bounds = nk_rect((float)child->bounds.x, (float)child->bounds.y, (float)child->bounds.w, (float)child->bounds.h);
				if (nk_begin(ctx, child->title, bounds, NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE)) {
					struct nk_panel *panel = nk_window_get_panel(ctx);
					child->bounds.x = (int)ctx->current->bounds.x;
					child->bounds.y = (int)ctx->current->bounds.y;
					child->bounds.w = (int)ctx->current->bounds.w;
					child->bounds.h = (int)ctx->current->bounds.h;
				}
				nk_end(ctx);
				child = child->next;
			}

			int width, height;
			SDL_GetWindowSize(presenter->window, &width, &height);
			pillow_case_draw(presenter, nk_rect(0, 0, width, height));
		}
	}
}

static pillow_case_child_t *pillow_case_presenter_embed(pillow_case_presenter_container_t *presenters, pillow_case_presenter_t *parent)
{
	pillow_case_child_container_t *children = presenters->children;
	size_t found = pillow_case_child_container_find_free(children);
	if (found) {
		pillow_case_child_t *entry = children->entries + found - 1;
		entry->parent = parent;
		entry->next = NULL;
		if (parent->children == NULL) {
			parent->children = entry;
		}
		else {
			entry->next = parent->children;
			parent->children = entry;
		}
		return entry;
	}
	return NULL;
}

static void pillow_case_presenter_detach(pillow_case_presenter_container_t *presenters, pillow_case_child_t *child)
{
	child->parent;

	pillow_case_child_t *previous = NULL;
	pillow_case_child_t *current = child->parent->children;
	while (current) {
		if (current == child) {
			if (previous != NULL) {
				previous->next = current->next;
			}
			if (current->parent->children == current) {
				current->parent->children = current->next;
			}
			current->next = NULL;
			current->title = "- BROKEN -";
			current->parent = NULL;
			break;
		}
		previous = current;
		current = current->next;
	}
}

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

static struct nk_rect pillow_case_menu_bar(struct pillow_nk_sdl_t *pillow_nk, pillow_case_editor_container_t *editors, struct nk_rect bounds)
{
	struct nk_context *ctx = nk(pillow_nk);
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
			while (pillow_case_editor_container_next(editors, &editor)) {
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

static void pillow_case_editors(struct pillow_nk_sdl_t *pillow_nk, pillow_case_editor_container_t *editors, struct nk_rect bounds)
{
	struct nk_context *ctx = nk(pillow_nk);

	pillow_case_editor_t *editor = NULL;
	while (pillow_case_editor_container_next(editors, &editor)) {
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

static bool pillow_event_watch(void *userdata, SDL_Event *event)
{
	pillow_case_executable_t *exe = (pillow_case_executable_t *)userdata;

	if (event->type == SDL_EVENT_WINDOW_EXPOSED) {
		pillow_case_presenter_container_draw(exe->presenters);
		return 1;
	}
	return 1;
}

static pillow_exectuable_status_t pillow_case_execute(pillow_executable_state_t *state)
{
	pillow_case_executable_t *exe = (pillow_case_executable_t *)state;

	// struct nk_context *ctx = nk(exe->nk);
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		pillow_case_presenter_container_push_event(exe->presenters, &event);
	}

	pillow_case_presenter_process_events(exe->presenters);

	for (size_t current_index = 0; current_index < exe->presenters->count; current_index++) {
		pillow_case_presenter_t *lhs = exe->presenters->entries + current_index;
		if (!pillow_case_presenter_used(lhs)) {
			continue;
		}

		for (size_t other_index = 0; other_index < exe->presenters->count; other_index++) {
			if (current_index == other_index)
				continue;
			pillow_case_presenter_t *rhs = exe->presenters->entries + other_index;
			if (!pillow_case_presenter_used(rhs)) {
				continue;
			}

			struct nk_recti nk_bounds;
			if (pillow_case_presenter_should_combine(lhs, rhs, &nk_bounds)) {
				if (rhs->children) {
					pillow_case_child_t *last = NULL;
					pillow_case_child_t *current = rhs->children;
					while (current) {
						current->bounds.x = current->bounds.x + nk_bounds.x;
						current->bounds.y = current->bounds.y + nk_bounds.y;
					
						current->parent = lhs;
						last = current;
						current = current->next;
					}

					if (lhs->children == NULL) {
						lhs->children = rhs->children;
						rhs->children = NULL;
					}
					else {
						last->next = lhs->children;
						lhs->children = last;
					}

				}

				// if(rhs->first != NULL) {
				//	if(lhs->first == NULL) {
				//		lhs->first = rhs->first;
				//		lhs->last = rhs->last;
				//		pillow_case_child_t* current = lhs->first;
				//		while(current) {
				//			current->parent = lhs;
				//			current = current->next;
				//			// TODO: reparent correctly
				//		}
				//	}
				//	else {
				//		// TODO: Append correctly
				//	}
				// }
				//  TODO: Handle all the ordering
				//  Add newest last!
				pillow_case_child_t *child = pillow_case_presenter_embed(exe->presenters, lhs);
				child->title = rhs->title;
				child->bounds = nk_bounds;
				// TODO: Recycle windows and renderers!
				pillow_case_presenter_container_remove(exe->presenters, exe->allocator, rhs->window);
			}
		}
	}

	for (size_t index = 0; index < exe->presenters->children->count; index++) {
		pillow_case_child_t *child = exe->presenters->children->entries + index;
		if (pillow_case_child_used(child)) {
			SDL_Rect bounds;
			if (pillow_case_presenter_should_detach(child, &bounds)) {
				pillow_case_presenter_t *presenter = pillow_case_presenter_container_add(exe->presenters, exe->allocator, child->title);
				SDL_SetWindowPosition(presenter->window, bounds.x, bounds.y);
				SDL_SetWindowSize(presenter->window, bounds.w, bounds.h);
				SDL_RaiseWindow(presenter->window);
				pillow_case_presenter_detach(exe->presenters, child);
			}
		}
	}

	pillow_case_presenter_container_draw(exe->presenters);

	return state->status;
}

static pillow_executable_state_t *pillow_case_start(pillow_allocator *allocator, pillow_registry_api *registry)
{
	pillow_case_executable_t exe = {0};
	exe.state.name = pillow_case;
	exe.state.status = pillow_executable_run;

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

	pillow_case_executable_t *out = pillow_register_value(registry, pillow_executable, exe);

	out->allocator = allocator;
	out->presenters = pillow_case_presenter_allocate(allocator, 64);
	pillow_case_presenter_container_add(out->presenters, allocator, "Window 1");
	pillow_case_presenter_container_add(out->presenters, allocator, "Window 2");
	pillow_case_presenter_container_add(out->presenters, allocator, "Window 3");
	pillow_case_presenter_container_add(out->presenters, allocator, "Window 4");

	SDL_AddEventWatch(pillow_event_watch, out);

	// SDL_SetWindowParent(pillow_case_presenter_container_add(out->presenters, allocator)->window, pillow_case_presenter_container_add(out->presenters, allocator)->window);

	// pillow_case_editor_container_add(&exe.editors, "Editor 0");
	// pillow_case_editor_container_add(&exe.editors, "Editor 1");
	// pillow_case_editor_container_add(&exe.editors, "Editor 2");
	// pillow_case_editor_container_add(&exe.editors, "Editor 3");
	// pillow_case_editor_container_add(&exe.editors, "Editor 4");

	return &out->state;
}

void pillow_case_shutdown(struct pillow_registry_api *registry, pillow_executable_state_t *state)
{
	pillow_case_executable_t *exe = (pillow_case_executable_t *)state;

	SDL_RemoveEventWatch(pillow_event_watch, exe);

	pillow_case_presenter_free(exe->allocator, exe->presenters);

	SDL_Quit();

	pillow_free(exe->allocator, exe);
}

static pillow_executable_interface pillow_case_implementation = {
	.start = pillow_case_start,
	.shutdown = pillow_case_shutdown,
	.execute = pillow_case_execute,
};
pillow_executable_interface *pillow_case_executable = &pillow_case_implementation;