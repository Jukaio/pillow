
#include "nuklear_default.h"
#include "pillow_allocator.h"
#include "pillow_array.h"

#include "SDL3/SDL_log.h"
#include "SDL3/SDL_render.h"

#include <assert.h>
#include <float.h>

#define pillow_case_editor_dock_node_capacity 256
#define pillow_case_editor_dock_window_capacity 128

#define nk_dock_buttons_row_count 5
#define nk_dock_buttons_column_count 5

typedef enum nk_rect_edge_type
{
	nk_rect_edge_none = 0,
	nk_rect_edge_left = 1,
	nk_rect_edge_right = 2,
	nk_rect_edge_top = 4,
	nk_rect_edge_bottom = 8,
} nk_rect_edge_type;

typedef struct pillow_nk_device_t
{
	struct nk_buffer cmds;
	struct nk_draw_null_texture tex_null;
	SDL_Texture *font_tex;
} pillow_nk_device_t;

typedef enum nk_dock_node_type_t
{
	nk_dock_invalid,
	nk_dock_space,
	nk_dock_window,
} nk_dock_node_type_t;

// I hate this node-based shit. It might be better to do it just forced...
typedef struct nk_dock_node_t
{
	nk_dock_node_type_t type;

	struct nk_rect public_bounds;
	struct nk_rect private_bounds;
} nk_dock_node_t;

typedef struct nk_dock_window_t
{
	nk_dock_node_t node;

	nk_hash hash;
	char title[NK_WINDOW_MAX_NAME];
} nk_dock_window_t;

typedef struct nk_dock_windows_container_t
{
	nk_dock_window_t entries[pillow_case_editor_dock_window_capacity];
	size_t count;
} nk_dock_windows_container_t;


typedef struct nk_dock_t
{
	nk_dock_windows_container_t windows;
} nk_dock_t;

typedef struct pillow_nk_sdl_t
{
	struct nk_context ctx;

	nk_dock_t dock;

	pillow_nk_device_t device;
	struct nk_font_atlas atlas;

} pillow_nk_sdl_t;

typedef struct nk_dock_adjustment_t
{
	// nk_dock_adjustment_type_t type;
	struct nk_vec2 value;
	struct nk_vec2 factor;
} nk_dock_adjustment_t;

typedef struct nk_dock_buttons_mask_t
{
	uint8_t values[nk_dock_buttons_column_count][nk_dock_buttons_row_count];
} nk_dock_buttons_mask_t;

const char *nk_rect_edge_to_string(nk_rect_edge_type edge)
{
	switch (edge) {
	case nk_rect_edge_none:
		return "nk_rect_edge_none";
	case nk_rect_edge_left:
		return "nk_rect_edge_left";
	case nk_rect_edge_right:
		return "nk_rect_edge_right";
	case nk_rect_edge_top:
		return "nk_rect_edge_top";
	case nk_rect_edge_bottom:
		return "nk_rect_edge_bottom";
	default:
		return "nk_rect_edge_unknown";
	}
}

static nk_dock_window_t *nk_dock_windows_container_add(nk_dock_windows_container_t *set, struct nk_window *window)
{
	nk_hash hash = window->name;
	const char *title = window->name_string;
	nk_hash slot = hash % pillow_array_size(set->entries);
	for (;;) {
		nk_dock_window_t *entry = set->entries + slot;
		if (entry->hash != 0) {
			if (strcmp(entry->title, title) == 0) {
				return entry;
			}
		}

		if (entry->hash == 0 || strcmp(entry->title, "\1") == 0) {
			memset(entry, 0, sizeof(*entry));
			entry->node.type = nk_dock_window;
			entry->hash = hash;
			int len = strlen(title) + 1;
			memcpy(entry->title, title, len < sizeof(entry->title) ? len : sizeof(entry->title));
			set->count = set->count + 1;
			return entry;
		}
		slot = (slot + 1) % pillow_array_size(set->entries);
	}
	return NULL;
}

static size_t nk_dock_windows_container_valid(nk_dock_windows_container_t *set, size_t index)
{
	if (index >= pillow_array_size(set->entries)) {
		return 0;
	}
	nk_dock_window_t *entry = set->entries + index;
	return entry->hash != 0 && strcmp(entry->title, "\1") != 0;
}

static size_t nk_dock_windows_container_find(nk_dock_windows_container_t *set, struct nk_window *window)
{
	nk_hash slot = window->name % pillow_array_size(set->entries);
	for (;;) {
		nk_dock_window_t *entry = set->entries + slot;
		// It is a tombstone!!
		if (strcmp(entry->title, "\1") != 0) {
			if (entry->hash == 0) {
				// Terminate
				return 0;
			}

			if (strcmp(entry->title, window->name_string) == 0) {
				return slot + 1;
			}
		}

		slot = (slot + 1) % pillow_array_size(set->entries);
	}
	return 0;
}

static size_t nk_dock_windows_container_remove(nk_dock_windows_container_t *set, struct nk_window *window)
{
	size_t at = nk_dock_windows_container_find(set, window);
	if (!at) {
		return 0;
	}

	nk_dock_window_t *entry = set->entries + at - 1;
	entry->title[0] = '\1';
	entry->title[1] = '\0';
	entry->hash = 0;
	set->count = set->count - 1;
	return at;
}

static void nk_sdl_clipboard_paste(nk_handle usr, struct nk_text_edit *edit)
{
	const char *text = SDL_GetClipboardText();
	if (text) {
		nk_textedit_paste(edit, text, nk_strlen(text));
		SDL_free((void *)text);
	}
	(void)usr;
}

static void nk_sdl_clipboard_copy(nk_handle usr, const char *text, int len)
{
	char *str = 0;
	(void)usr;
	if (!len) {
		return;
	}

	str = (char *)malloc((size_t)len + 1);
	if (!str) {
		return;
	}

	memcpy(str, text, (size_t)len);
	str[len] = '\0';
	SDL_SetClipboardText(str);
	free(str);
}

static void nk_sdl_init(struct nk_context *ctx, struct nk_buffer *commands)
{
	nk_init_default(ctx, 0);
	ctx->clip.copy = nk_sdl_clipboard_copy;
	ctx->clip.paste = nk_sdl_clipboard_paste;
	ctx->clip.userdata = nk_handle_ptr(0);
	nk_buffer_init_default(commands);
}

static struct nk_font_atlas *nk_sdl_font_stash_begin(struct nk_font_atlas *atlas)
{
	nk_font_atlas_init_default(atlas);
	nk_font_atlas_begin(atlas);
	return atlas;
}

static SDL_Texture *nk_sdl_device_upload_atlas(SDL_Renderer *renderer, const void *image, int width, int height)
{
	SDL_Texture *font_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, width, height);
	if (font_texture == NULL) {
		SDL_Log("error creating texture");
		return;
	}
	SDL_UpdateTexture(font_texture, NULL, image, 4 * width);
	SDL_SetTextureBlendMode(font_texture, SDL_BLENDMODE_BLEND);
	return font_texture;
}

static SDL_Texture *nk_sdl_font_stash_end(struct nk_context *ctx, struct nk_font_atlas *atlas, SDL_Renderer *renderer, struct nk_draw_null_texture *null_texture)
{
	const void *image;
	int w, h;
	image = nk_font_atlas_bake(atlas, &w, &h, NK_FONT_ATLAS_RGBA32);
	SDL_Texture *font_texture = nk_sdl_device_upload_atlas(renderer, image, w, h);
	nk_font_atlas_end(atlas, nk_handle_ptr(font_texture), null_texture);
	nk_style_set_font(ctx, &atlas->default_font->handle);
	return font_texture;
}

int nk_sdl_handle_event(pillow_nk_sdl_t *nk_sdl, SDL_Event *evt)
{
	struct nk_context *ctx = &nk_sdl->ctx;
	int ctrl_down = SDL_GetModState() & SDL_KMOD_CTRL;
	static int insert_toggle = 0;

	switch (evt->type) {
	case SDL_EVENT_KEY_UP: /* KEYUP & KEYDOWN share same routine */
	case SDL_EVENT_KEY_DOWN: {
		int down = evt->type == SDL_EVENT_KEY_DOWN;
		switch (evt->key.key) {
		case SDLK_LCTRL:
			nk_input_key(ctx, NK_KEY_CTRL, down);
			break;
		case SDLK_RCTRL:
			nk_input_key(ctx, NK_KEY_CTRL, down);
			break;
		case SDLK_RSHIFT: /* RSHIFT & LSHIFT share same routine */
		case SDLK_LSHIFT:
			nk_input_key(ctx, NK_KEY_SHIFT, down);
			break;
		case SDLK_DELETE:
			nk_input_key(ctx, NK_KEY_DEL, down);
			break;

		case SDLK_KP_ENTER:
		case SDLK_RETURN:
			nk_input_key(ctx, NK_KEY_ENTER, down);
			break;

		case SDLK_TAB:
			nk_input_key(ctx, NK_KEY_TAB, down);
			break;
		case SDLK_BACKSPACE:
			nk_input_key(ctx, NK_KEY_BACKSPACE, down);
			break;
		case SDLK_HOME:
			nk_input_key(ctx, NK_KEY_TEXT_START, down);
			nk_input_key(ctx, NK_KEY_SCROLL_START, down);
			break;
		case SDLK_END:
			nk_input_key(ctx, NK_KEY_TEXT_END, down);
			nk_input_key(ctx, NK_KEY_SCROLL_END, down);
			break;
		case SDLK_PAGEDOWN:
			nk_input_key(ctx, NK_KEY_SCROLL_DOWN, down);
			break;
		case SDLK_PAGEUP:
			nk_input_key(ctx, NK_KEY_SCROLL_UP, down);
			break;
		case SDLK_Z:
			nk_input_key(ctx, NK_KEY_TEXT_UNDO, down && ctrl_down);
			break;
		case SDLK_R:
			nk_input_key(ctx, NK_KEY_TEXT_REDO, down && ctrl_down);
			break;
		case SDLK_C:
			nk_input_key(ctx, NK_KEY_COPY, down && ctrl_down);
			break;
		case SDLK_V:
			nk_input_key(ctx, NK_KEY_PASTE, down && ctrl_down);
			break;
		case SDLK_X:
			nk_input_key(ctx, NK_KEY_CUT, down && ctrl_down);
			break;
		case SDLK_B:
			nk_input_key(ctx, NK_KEY_TEXT_LINE_START, down && ctrl_down);
			break;
		case SDLK_E:
			nk_input_key(ctx, NK_KEY_TEXT_LINE_END, down && ctrl_down);
			break;
		case SDLK_UP:
			nk_input_key(ctx, NK_KEY_UP, down);
			break;
		case SDLK_DOWN:
			nk_input_key(ctx, NK_KEY_DOWN, down);
			break;
		case SDLK_ESCAPE:
			nk_input_key(ctx, NK_KEY_TEXT_RESET_MODE, down);
			break;
		case SDLK_INSERT:
			if (down)
				insert_toggle = !insert_toggle;
			if (insert_toggle) {
				nk_input_key(ctx, NK_KEY_TEXT_INSERT_MODE, down);
			}
			else {
				nk_input_key(ctx, NK_KEY_TEXT_REPLACE_MODE, down);
			}
			break;
		case SDLK_A:
			if (ctrl_down)
				nk_input_key(ctx, NK_KEY_TEXT_SELECT_ALL, down);
			break;
		case SDLK_LEFT:
			if (ctrl_down)
				nk_input_key(ctx, NK_KEY_TEXT_WORD_LEFT, down);
			else
				nk_input_key(ctx, NK_KEY_LEFT, down);
			break;
		case SDLK_RIGHT:
			if (ctrl_down)
				nk_input_key(ctx, NK_KEY_TEXT_WORD_RIGHT, down);
			else
				nk_input_key(ctx, NK_KEY_RIGHT, down);
			break;
		}
	}
		return 1;

	case SDL_EVENT_MOUSE_BUTTON_UP: /* MOUSEBUTTONUP & MOUSEBUTTONDOWN share same routine */
	case SDL_EVENT_MOUSE_BUTTON_DOWN: {
		int down = evt->type == SDL_EVENT_MOUSE_BUTTON_DOWN;
		const int x = evt->button.x, y = evt->button.y;
		switch (evt->button.button) {
		case SDL_BUTTON_LEFT:
			if (evt->button.clicks > 1)
				nk_input_button(ctx, NK_BUTTON_DOUBLE, x, y, down);
			nk_input_button(ctx, NK_BUTTON_LEFT, x, y, down);
			break;
		case SDL_BUTTON_MIDDLE:
			nk_input_button(ctx, NK_BUTTON_MIDDLE, x, y, down);
			break;
		case SDL_BUTTON_RIGHT:
			nk_input_button(ctx, NK_BUTTON_RIGHT, x, y, down);
			break;
		}
	}
		return 1;

	case SDL_EVENT_MOUSE_MOTION:
		if (ctx->input.mouse.grabbed) {
			int x = (int)ctx->input.mouse.prev.x, y = (int)ctx->input.mouse.prev.y;
			nk_input_motion(ctx, x + evt->motion.xrel, y + evt->motion.yrel);
		}
		else {
			nk_input_motion(ctx, evt->motion.x, evt->motion.y);
		}
		return 1;

	case SDL_EVENT_TEXT_INPUT: {
		nk_glyph glyph;
		memcpy(glyph, evt->text.text, NK_UTF_SIZE);
		nk_input_glyph(ctx, glyph);
	}
		return 1;

	case SDL_EVENT_MOUSE_WHEEL:
		nk_input_scroll(ctx, nk_vec2(evt->wheel.x, evt->wheel.y));
		return 1;
	}
	return 0;
}

void nk_sdl_draw(pillow_nk_sdl_t *nk_sdl, SDL_Renderer *renderer)
{

	pillow_nk_device_t *device = &nk_sdl->device;
	struct nk_context *ctx = &nk_sdl->ctx;

	const int vs = sizeof(SDL_Vertex);
	static const size_t vp = NK_OFFSETOF(SDL_Vertex, position);
	static const size_t vt = NK_OFFSETOF(SDL_Vertex, tex_coord);
	static const size_t vc = NK_OFFSETOF(SDL_Vertex, color);

	const struct nk_draw_command *cmd;
	const nk_draw_index *offset = NULL;
	struct nk_buffer vbuf, ebuf;
	struct nk_convert_config config;
	static const struct nk_draw_vertex_layout_element vertex_layout[] = {{NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(SDL_Vertex, position)},
	                                                                     {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(SDL_Vertex, tex_coord)},
	                                                                     {NK_VERTEX_COLOR, NK_FORMAT_R32G32B32A32_FLOAT, NK_OFFSETOF(SDL_Vertex, color)},
	                                                                     {NK_VERTEX_LAYOUT_END}};

	memset(&config, 0, sizeof(config));
	config.vertex_layout = vertex_layout;
	config.vertex_size = sizeof(SDL_Vertex);
	config.vertex_alignment = NK_ALIGNOF(SDL_Vertex);
	config.tex_null = device->tex_null;
	config.circle_segment_count = 22;
	config.curve_segment_count = 22;
	config.arc_segment_count = 22;
	config.global_alpha = 1.0f;
	config.shape_AA = NK_ANTI_ALIASING_ON;
	config.line_AA = NK_ANTI_ALIASING_ON;

	nk_buffer_init_default(&vbuf);
	nk_buffer_init_default(&ebuf);
	nk_convert(ctx, &device->cmds, &vbuf, &ebuf, &config);

	offset = (const nk_draw_index *)nk_buffer_memory_const(&ebuf);

	SDL_Rect saved_clip;
	SDL_GetRenderClipRect(renderer, &saved_clip);
	nk_bool clipping_enabled = SDL_RenderClipEnabled(renderer);

	nk_draw_foreach(cmd, ctx, &device->cmds)
	{

		if (!cmd->elem_count)
			continue;

		{
			SDL_Rect r;
			r.x = cmd->clip_rect.x;
			r.y = cmd->clip_rect.y;
			r.w = cmd->clip_rect.w;
			r.h = cmd->clip_rect.h;
			SDL_SetRenderClipRect(renderer, &r);
		}

		{
			const void *vertices = nk_buffer_memory_const(&vbuf);
			// SDL_RenderGeometry(renderer, (SDL_Texture*)cmd->texture.ptr, (const SDL_Vertex*)vertices, vbuf.needed / vs, (void*)offset, cmd->elem_count);
			//  Needed if we change nk_draw_index to be two bytes!
			SDL_RenderGeometryRaw(renderer,
			                      (SDL_Texture *)cmd->texture.ptr,
			                      (const float *)((const nk_byte *)vertices + vp),
			                      vs,
			                      (const SDL_FColor *)((const nk_byte *)vertices + vc),
			                      vs,
			                      (const float *)((const nk_byte *)vertices + vt),
			                      vs,
			                      (vbuf.needed / vs),
			                      (void *)offset,
			                      cmd->elem_count,
			                      sizeof(nk_draw_index));

			offset += cmd->elem_count;
		}
	}

	SDL_SetRenderClipRect(renderer, &saved_clip);
	if (!clipping_enabled) {
		SDL_SetRenderClipRect(renderer, NULL);
	}

	nk_clear(ctx);
	nk_buffer_clear(&device->cmds);
	nk_buffer_free(&vbuf);
	nk_buffer_free(&ebuf);
}

struct pillow_nk_sdl_t *nk_sdl_allocate(pillow_allocator *allocator, SDL_Renderer *renderer)
{
	struct nk_font_config config = nk_font_config(0);

	pillow_nk_sdl_t *nk = (pillow_nk_sdl_t *)pillow_malloc(allocator, sizeof(*nk));
	memset(nk, 0, sizeof(*nk));

	pillow_nk_device_t *device = &nk->device;
	nk_sdl_init(&nk->ctx, &device->cmds);
	struct nk_font_atlas *atlas = nk_sdl_font_stash_begin(&nk->atlas);
	struct nk_font *font = nk_font_atlas_add_default(atlas, 13.0f, &config);
	device->font_tex = nk_sdl_font_stash_end(&nk->ctx, atlas, renderer, &device->tex_null);
	nk_style_set_font(&nk->ctx, &font->handle);

	nk_set_style(&nk->ctx, nk_theme_red);
	return nk;
}

void nk_sdl_free(struct pillow_allocator *allocator, struct pillow_nk_sdl_t *nk)
{
	pillow_free(allocator, nk);
}

struct nk_context *nk(struct pillow_nk_sdl_t *nk_sdl)
{
	return &nk_sdl->ctx;
}

struct pillow_nk_sdl_t *pillow_nk(struct nk_context *ctx)
{
	return (pillow_nk_sdl_t *)ctx;
}

// NK EXTENSIONS!

static int nk_dock_bounds_contains(const struct nk_rect *bigger, struct nk_rect *smaller)
{
	if (smaller->x < bigger->x) {
		return 0;
	}
	if (smaller->y < bigger->y) {
		return 0;
	}
	if ((smaller->x + smaller->w) > (bigger->x + bigger->w)) {
		return 0;
	}
	if ((smaller->y + smaller->h) > (bigger->y + bigger->h)) {
		return 0;
	}
	return 1;
}

static int nk_release_button_symbol(struct nk_context *ctx, enum nk_symbol_type symbol)
{
	int was_released = 0;
	struct nk_rect rect = nk_widget_bounds(ctx);
	if (nk_input_is_mouse_hovering_rect(&ctx->input, rect)) {
		if (nk_input_is_mouse_released(&ctx->input, NK_BUTTON_LEFT)) {
			was_released = 1;
		}
	}

	nk_button_symbol(ctx, symbol);
	return was_released;
}

static int nk_layout_dock_buttons(struct pillow_nk_sdl_t *pillow_ctx, const nk_dock_buttons_mask_t *deactivate, struct nk_dock_adjustment_t *adjustment)
{
	typedef enum dock_entry_type_t
	{
		dock_entry_spacer,
		dock_entry_static,
		dock_entry_dynamic,
	} dock_entry_type_t;

	typedef struct dock_column_entry_config_t
	{
		struct nk_vec2 value;
		struct nk_vec2 factor;
	} dock_column_entry_config_t;

	typedef struct dock_column_config_t
	{
		const char *title; // Too lazy to make it "dynamic"
		dock_column_entry_config_t entries[nk_dock_buttons_row_count];
	} dock_column_config_t;

	static const dock_column_config_t columns[nk_dock_buttons_column_count] = {
		{
			.title = "Dock Column 0",
			.entries = {{.value = {0.0f, 0.0f}, .factor = {0.5f, 0.5f}}, {0}, {0}, {0}, {.value = {0.0f, 0.5f}, .factor = {0.5f, 0.5f}}},
		},
		{
			.title = "Dock Column 1",
			.entries = {{0}, {0}, {.value = {0.0f, 0.0f}, .factor = {0.5f, 1.0f}}, {0}, {0}},
		},
		{
			.title = "Dock Column 2",
			.entries = {{0}, {.value = {0.0f, 0.0}, .factor = {1.0f, 0.5f}}, {.value = {0.0f, 0.0f}, .factor = {1.0f, 1.0f}}, {.value = {0.0f, 0.5f}, .factor = {1.0f, 0.5f}}, {0}},
		},
		{
			.title = "Dock Column 3",
			.entries = {{0}, {0}, {.value = {0.5f, 0.0f}, .factor = {0.5f, 1.0f}}, {0}, {0}},
		},
		{
			.title = "Dock Column 4",
			.entries = {{.value = {0.5f, 0.0f}, .factor = {0.5f, 0.5f}}, {0}, {0}, {0}, {.value = {0.5f, 0.5f}, .factor = {0.5f, 0.5f}}},
		},
	};

	struct nk_context *ctx = nk(pillow_ctx);
	struct nk_panel *panel = nk_window_get_panel(ctx);
	const struct nk_rect bounds = panel->bounds;
	int result = 0;
	nk_layout_row_dynamic(ctx, bounds.h, pillow_array_size(columns));
	for (size_t column_index = 0; column_index < pillow_array_size(columns); column_index++) {
		dock_column_config_t *column = columns + column_index;

		if (nk_group_begin(ctx, column->title, NK_WINDOW_NO_SCROLLBAR)) {
			nk_layout_row_dynamic(ctx, 0.0f, 1);
			for (size_t entry_index = 0; entry_index < pillow_array_size(column->entries); entry_index++) {

				const uint8_t is_off = deactivate->values[column_index][entry_index];

				dock_column_entry_config_t *entry = column->entries + entry_index;
				struct nk_vec2 value = entry->value;
				struct nk_vec2 factor = entry->factor;
				if (is_off || value.x == 0 && value.y == 0 && factor.x == 0 && factor.y == 0) {
					nk_spacer(ctx);
				}
				else {
					if (nk_release_button_symbol(ctx, NK_SYMBOL_RECT_OUTLINE)) {
						adjustment->value = entry->value;
						adjustment->factor = entry->factor;
						result = 1;
					}
				}
			}
			nk_group_end(ctx);
		}
	}
	return result;
}

static float nk_dock_remap_coordinates(float value, float old_min, float old_max, float new_min, float new_max)
{
	return (value - old_min) * (new_max - new_min) / (old_max - old_min) + new_min;
}

struct nk_dock_window_t *nk_dock_hovered_window(pillow_nk_sdl_t *pillow_ctx, struct nk_window **hovered)
{
	// TODO: This function is mad nasty
	struct nk_context *ctx = nk(pillow_ctx);
	struct nk_window *iter = ctx->end;
	size_t found;
	while (iter) {
		found = nk_dock_windows_container_find(&pillow_ctx->dock.windows, iter);
		if (found) {
			if (!(iter->flags & NK_WINDOW_HIDDEN)) {
				if (iter->popup.active && iter->popup.win && nk_input_is_mouse_hovering_rect(&ctx->input, iter->popup.win->bounds))
					break;

				if (iter->flags & NK_WINDOW_MINIMIZED) {
					struct nk_rect header = iter->bounds;
					header.h = ctx->style.font->height + 2 * ctx->style.window.header.padding.y;
					if (nk_input_is_mouse_hovering_rect(&ctx->input, header))
						break;
				}
				if (nk_input_is_mouse_hovering_rect(&ctx->input, iter->bounds)) {
					break;
				}
			}
		}
		iter = iter->prev;
	}
	*hovered = iter;
	if (!iter) {
		return NULL;
	}
	return pillow_ctx->dock.windows.entries + found - 1;
}

struct nk_rect nk_rect_remainder(struct nk_rect primary, struct nk_rect sub)
{
	// Sub shall be a subset of the primary rect!! :) Maybe assert on contain?
	// TODO: This function might be problematic
	struct nk_vec2 lhs_min = nk_vec2(primary.x, primary.y);
	struct nk_vec2 lhs_max = nk_vec2(lhs_min.x + primary.w, lhs_min.y + primary.h);

	struct nk_vec2 rhs_min = nk_vec2(sub.x, sub.y);
	struct nk_vec2 rhs_max = nk_vec2(rhs_min.x + sub.w, rhs_min.y + sub.h);

	struct nk_vec2 delta_min = nk_vec2(lhs_min.x - rhs_min.x, lhs_min.y - rhs_min.y);
	struct nk_vec2 delta_max = nk_vec2(lhs_max.x - rhs_max.x, lhs_max.y - rhs_max.y);

	struct nk_rect result = sub;
	result.x = result.x + delta_max.x + delta_min.x;
	result.y = result.y + delta_max.y + delta_min.y;

	return result;
}

int nk_float_appox(float lhs, float rhs)
{
	const float close_enough = 1e-5f;
	return fabsf(lhs - rhs) < close_enough;
}

nk_rect_edge_type nk_rect_exact_shared_edge(struct nk_rect lhs, struct nk_rect rhs)
{
	// rhs.x == left
	// lhs.x == left
	// lhs.y == top
	// rhs.y == top

	const float lhs_right = lhs.x + lhs.w;
	const float lhs_bottom = lhs.y + lhs.h;
	const float rhs_right = rhs.x + rhs.w;
	const float rhs_bottom = rhs.y + rhs.h;

	if (nk_float_appox(lhs.y, rhs.y) && nk_float_appox(lhs_bottom, rhs_bottom)) {
		if (nk_float_appox(lhs.x, rhs_right)) {
			return nk_rect_edge_left;
		}
		if (nk_float_appox(lhs_right, rhs.x)) {
			return nk_rect_edge_right;
		}
	}

	if (nk_float_appox(lhs.x, rhs.x) && nk_float_appox(lhs_right, rhs_right)) {
		if (nk_float_appox(lhs.y, rhs_bottom)) {
			return nk_rect_edge_top;
		}
		if (nk_float_appox(lhs_bottom, rhs.y)) {
			return nk_rect_edge_bottom;
		}
	}

	return nk_rect_edge_none;
}

struct nk_rect nk_rect_extend(struct nk_rect lhs, struct nk_rect rhs)
{
	struct nk_vec2 lhs_min = nk_vec2(lhs.x, lhs.y);
	struct nk_vec2 lhs_max = nk_vec2(lhs_min.x + lhs.w, lhs_min.y + lhs.h);

	struct nk_vec2 rhs_min = nk_vec2(rhs.x, rhs.y);
	struct nk_vec2 rhs_max = nk_vec2(rhs_min.x + rhs.w, rhs_min.y + rhs.h);

	struct nk_vec2 max = nk_vec2(fmaxf(lhs_max.x, rhs_max.x), fmaxf(lhs_max.y, rhs_max.y));

	struct nk_rect result = lhs;
	result.x = fminf(lhs.x, rhs.x);
	result.y = fminf(lhs.y, rhs.y);
	result.w = max.x - result.x;
	result.h = max.y - result.y;
	return result;
}

void nk_dock_resize(pillow_nk_sdl_t *pillow_ctx, struct nk_rect bounds)
{
	nk_dock_windows_container_t *windows = &pillow_ctx->dock.windows;

	// First we try resizing on full edges - This is the best case scenario!
	for (size_t index = 0; index < pillow_array_size(windows->entries); index++) {
		if (nk_dock_windows_container_valid(windows, index)) {
			nk_dock_window_t *entry = windows->entries + index;
			nk_rect_edge_type edge_type = nk_rect_exact_shared_edge(bounds, entry->node.private_bounds);
			if (edge_type != nk_rect_edge_none) {
				struct nk_rect result = nk_rect_extend(bounds, entry->node.private_bounds);
				entry->node.private_bounds = result;
				break;
			}
		}
	}

	// The side with the MOST fitting smaller windows wins!
	// We pretty much project a side up, left and right and check
	// 1. collect windows in each direction
	// 2. create an accumuator set to the width/height (depends on direction)
	// 3. subtract the width/height from each accumaltor for each direction
	// 4. whoever hits 0.0 wins
	// 5. The following can not hit 0 for any window.
	// X X Y
	// Z W Y
	// Z R R
	// Is this even possible? 
}

int nk_dock_popup(struct nk_context *ctx, float x, float y, float w, float h)
{
	struct nk_window *target = ctx->current;
	struct nk_rect title_bounds = target->bounds;
	title_bounds.h = target->layout->header_height;

	// These bounds are incoming! Everything else uses window bounds, bit different! :)
	struct nk_rect bounds = nk_rect(x, y, w, h);

	pillow_nk_sdl_t *pillow_ctx = pillow_nk(ctx);

	// TODO: Let's see if we can change the active check to a if(ctx->active) { /* Do stuff */} thing

	if (ctx->active == ctx->current) {
		if (!nk_input_is_key_down(&ctx->input, NK_KEY_CTRL)) {
			if (nk_input_is_mouse_hovering_rect(&ctx->input, title_bounds)) {
				int released = nk_input_is_mouse_released(&ctx->input, NK_BUTTON_LEFT);
				if (released || nk_input_is_mouse_down(&ctx->input, NK_BUTTON_LEFT)) {
					const float button_width = 120.0f;
					const float button_height = 92.0f;
					const float w = button_width * 3;
					const float h = button_height * 3;

					struct nk_rect *original_bounds = &bounds;
					struct nk_window *hovered = NULL;

					struct nk_dock_window_t *hovered_entry = nk_dock_hovered_window(pillow_ctx, &hovered);
					if (hovered) {
						original_bounds = &hovered->bounds;
					}

					// Make global, simply subtract the relative location
					float x = -target->bounds.x - (w * 0.5f) + (original_bounds->w * 0.5f);
					float y = -target->bounds.y - (h * 0.5f) + (original_bounds->h * 0.5f);
					if (hovered) {
						x = x + original_bounds->x;
						y = y + original_bounds->y;
					}

					struct nk_rect popup_rect = nk_rect(x, y, w, h);

					nk_style_push_float(ctx, &ctx->style.window.popup_padding, 8.0f);
					if (nk_popup_begin(ctx, NK_POPUP_DYNAMIC, "DOCK-POPUP", NK_WINDOW_PASSTHROUGH | NK_WINDOW_NO_SCROLLBAR, popup_rect)) {
						nk_dock_buttons_mask_t mask = {0};
						mask.values[2][2] = 0xFF;
						mask.values[0][0] = 0xFF;
						mask.values[0][4] = 0xFF;
						mask.values[4][0] = 0xFF;
						mask.values[4][4] = 0xFF;
						if (hovered == NULL) {
							memset(mask.values, 0xFF, sizeof(mask.values));
							mask.values[2][2] = 0;
						}

						struct nk_dock_adjustment_t adjustment;
						if (nk_layout_dock_buttons(pillow_ctx, &mask, &adjustment)) {
							nk_dock_window_t *entry = nk_dock_windows_container_add(&pillow_ctx->dock.windows, target);
							if (entry) {
								entry->node.private_bounds.w = original_bounds->w * adjustment.factor.x;
								entry->node.private_bounds.h = original_bounds->h * adjustment.factor.y;
								entry->node.private_bounds.x = original_bounds->x + (adjustment.value.x * original_bounds->w);
								entry->node.private_bounds.y = original_bounds->y + (adjustment.value.y * original_bounds->h);

								if (hovered_entry) {
									struct nk_rect other_bounds = nk_rect_remainder(*original_bounds, entry->node.private_bounds);
									hovered_entry->node.private_bounds = other_bounds;
								}
							}
						}
						nk_popup_end(ctx);
					}
					nk_style_pop_float(ctx);
				}
			}
		}
	}
	{
		const size_t found = nk_dock_windows_container_find(&pillow_ctx->dock.windows, target);
		if (found) {
			// If position or size changes that has not been driven by docking, it does not count as docked anymore!
			nk_dock_window_t *entry = pillow_ctx->dock.windows.entries + found - 1;

			const struct nk_rect *prev = &entry->node.public_bounds;
			const struct nk_rect *control = &entry->node.private_bounds;
			if (prev->x != control->x || prev->y != control->y || prev->w != control->w || prev->h != control->h) {
				entry->node.public_bounds = *control;
				target->bounds = entry->node.public_bounds;

				prev = control;
			}

			if (prev->x == target->bounds.x && prev->y == target->bounds.y && prev->w == target->bounds.w && prev->h == target->bounds.h) {
				return 1;
			}
			
			// TODO: When we undock, we need to resize!
			
			// Undock
			struct nk_rect resize_bounds = entry->node.private_bounds;
			nk_dock_windows_container_remove(&pillow_ctx->dock.windows, target);
			nk_dock_resize(pillow_ctx, resize_bounds);
		}
	}
	return 0;
}
