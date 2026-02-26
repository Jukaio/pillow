
#include "nuklear_default.h"
#include "pillow_allocator.h"
#include "SDL3/SDL_render.h"

typedef struct pillow_nk_device_t {
	struct nk_buffer cmds;
	struct nk_draw_null_texture tex_null;
	SDL_Texture* font_tex;
}pillow_nk_device_t;

typedef struct pillow_nk_input{
	struct nk_mouse_button buttons[NK_BUTTON_MAX];

}pillow_nk_input;

typedef struct pillow_nk_sdl_t {
	// So we can cast it back and forth!
	struct nk_context ctx;

	pillow_nk_input prev;

	pillow_nk_device_t device;
	struct nk_font_atlas atlas;
}pillow_nk_sdl_t;


static void nk_sdl_clipboard_paste(nk_handle usr, struct nk_text_edit* edit)
{
	const char* text = SDL_GetClipboardText();
	if (text) {
		nk_textedit_paste(edit, text, nk_strlen(text));
		SDL_free((void*)text);
	}
	(void)usr;
}

static void nk_sdl_clipboard_copy(nk_handle usr, const char* text, int len)
{
	char* str = 0;
	(void)usr;
	if (!len) {
		return;
	}

	str = (char*)malloc((size_t)len + 1);
	if (!str) {
		return;
	}

	memcpy(str, text, (size_t)len);
	str[len] = '\0';
	SDL_SetClipboardText(str);
	free(str);
}

static void nk_sdl_init(struct nk_context* ctx, struct nk_buffer* commands)
{
	nk_init_default(ctx, 0);
	ctx->clip.copy = nk_sdl_clipboard_copy;
	ctx->clip.paste = nk_sdl_clipboard_paste;
	ctx->clip.userdata = nk_handle_ptr(0);
	nk_buffer_init_default(commands);
}

static struct nk_font_atlas* nk_sdl_font_stash_begin(struct nk_font_atlas* atlas)
{
	nk_font_atlas_init_default(atlas);
	nk_font_atlas_begin(atlas);
	return atlas;
}

static SDL_Texture* nk_sdl_device_upload_atlas(SDL_Renderer* renderer, const void* image, int width, int height)
{
	SDL_Texture* font_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, width, height);
	if (font_texture == NULL) {
		SDL_Log("error creating texture");
		return;
	}
	SDL_UpdateTexture(font_texture, NULL, image, 4 * width);
	SDL_SetTextureBlendMode(font_texture, SDL_BLENDMODE_BLEND);
	return font_texture;
}

static SDL_Texture* nk_sdl_font_stash_end(struct nk_context* ctx, struct nk_font_atlas* atlas, SDL_Renderer* renderer, struct nk_draw_null_texture* null_texture)
{
	const void* image; int w, h;
	image = nk_font_atlas_bake(atlas, &w, &h, NK_FONT_ATLAS_RGBA32);
	SDL_Texture* font_texture = nk_sdl_device_upload_atlas(renderer, image, w, h);
	nk_font_atlas_end(atlas, nk_handle_ptr(font_texture), null_texture);
	nk_style_set_font(ctx, &atlas->default_font->handle);
	return font_texture;
}

int nk_sdl_handle_event(pillow_nk_sdl_t* nk_sdl, SDL_Event* evt)
{
	struct nk_context* ctx = &nk_sdl->ctx;
	int ctrl_down = SDL_GetModState() & SDL_KMOD_CTRL;
	static int insert_toggle = 0;

	switch (evt->type)
	{
	case SDL_EVENT_KEY_UP: /* KEYUP & KEYDOWN share same routine */
	case SDL_EVENT_KEY_DOWN:
	{
		int down = evt->type == SDL_EVENT_KEY_DOWN;
		switch (evt->key.key)
		{
		case SDLK_RSHIFT: /* RSHIFT & LSHIFT share same routine */
		case SDLK_LSHIFT:    nk_input_key(ctx, NK_KEY_SHIFT, down); break;
		case SDLK_DELETE:    nk_input_key(ctx, NK_KEY_DEL, down); break;

		case SDLK_KP_ENTER:
		case SDLK_RETURN:    nk_input_key(ctx, NK_KEY_ENTER, down); break;

		case SDLK_TAB:       nk_input_key(ctx, NK_KEY_TAB, down); break;
		case SDLK_BACKSPACE: nk_input_key(ctx, NK_KEY_BACKSPACE, down); break;
		case SDLK_HOME:      nk_input_key(ctx, NK_KEY_TEXT_START, down);
			nk_input_key(ctx, NK_KEY_SCROLL_START, down); break;
		case SDLK_END:       nk_input_key(ctx, NK_KEY_TEXT_END, down);
			nk_input_key(ctx, NK_KEY_SCROLL_END, down); break;
		case SDLK_PAGEDOWN:  nk_input_key(ctx, NK_KEY_SCROLL_DOWN, down); break;
		case SDLK_PAGEUP:    nk_input_key(ctx, NK_KEY_SCROLL_UP, down); break;
		case SDLK_Z:         nk_input_key(ctx, NK_KEY_TEXT_UNDO, down && ctrl_down); break;
		case SDLK_R:         nk_input_key(ctx, NK_KEY_TEXT_REDO, down && ctrl_down); break;
		case SDLK_C:         nk_input_key(ctx, NK_KEY_COPY, down && ctrl_down); break;
		case SDLK_V:         nk_input_key(ctx, NK_KEY_PASTE, down && ctrl_down); break;
		case SDLK_X:         nk_input_key(ctx, NK_KEY_CUT, down && ctrl_down); break;
		case SDLK_B:         nk_input_key(ctx, NK_KEY_TEXT_LINE_START, down && ctrl_down); break;
		case SDLK_E:         nk_input_key(ctx, NK_KEY_TEXT_LINE_END, down && ctrl_down); break;
		case SDLK_UP:        nk_input_key(ctx, NK_KEY_UP, down); break;
		case SDLK_DOWN:      nk_input_key(ctx, NK_KEY_DOWN, down); break;
		case SDLK_ESCAPE:    nk_input_key(ctx, NK_KEY_TEXT_RESET_MODE, down); break;
		case SDLK_INSERT:
			if (down) insert_toggle = !insert_toggle;
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
			else nk_input_key(ctx, NK_KEY_LEFT, down);
			break;
		case SDLK_RIGHT:
			if (ctrl_down)
				nk_input_key(ctx, NK_KEY_TEXT_WORD_RIGHT, down);
			else nk_input_key(ctx, NK_KEY_RIGHT, down);
			break;
		}
	}
	return 1;

	case SDL_EVENT_MOUSE_BUTTON_UP: /* MOUSEBUTTONUP & MOUSEBUTTONDOWN share same routine */
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	{
		int down = evt->type == SDL_EVENT_MOUSE_BUTTON_DOWN;
		const int x = evt->button.x, y = evt->button.y;
		switch (evt->button.button)
		{
		case SDL_BUTTON_LEFT:
			if (evt->button.clicks > 1)
				nk_input_button(ctx, NK_BUTTON_DOUBLE, x, y, down);
			nk_input_button(ctx, NK_BUTTON_LEFT, x, y, down); break;
		case SDL_BUTTON_MIDDLE: nk_input_button(ctx, NK_BUTTON_MIDDLE, x, y, down); break;
		case SDL_BUTTON_RIGHT:  nk_input_button(ctx, NK_BUTTON_RIGHT, x, y, down); break;
		}
	}
	return 1;

	case SDL_EVENT_MOUSE_MOTION:
		if (ctx->input.mouse.grabbed) {
			int x = (int)ctx->input.mouse.prev.x, y = (int)ctx->input.mouse.prev.y;
			nk_input_motion(ctx, x + evt->motion.xrel, y + evt->motion.yrel);
		}
		else nk_input_motion(ctx, evt->motion.x, evt->motion.y);
		return 1;

	case SDL_EVENT_TEXT_INPUT:
	{
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

void nk_sdl_draw(pillow_nk_sdl_t* nk_sdl, SDL_Renderer* renderer) {

	pillow_nk_device_t* device = &nk_sdl->device;
	struct nk_context* ctx = &nk_sdl->ctx;

	const int vs = sizeof(SDL_Vertex);
	static const size_t vp = NK_OFFSETOF(SDL_Vertex, position);
	static const size_t vt = NK_OFFSETOF(SDL_Vertex, tex_coord);
	static const size_t vc = NK_OFFSETOF(SDL_Vertex, color);

	const struct nk_draw_command* cmd;
	const nk_draw_index* offset = NULL;
	struct nk_buffer vbuf, ebuf;
	struct nk_convert_config config;
	static const struct nk_draw_vertex_layout_element vertex_layout[] = {
		{NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(SDL_Vertex, position)},
		{NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(SDL_Vertex, tex_coord)},
		{NK_VERTEX_COLOR, NK_FORMAT_R32G32B32A32_FLOAT, NK_OFFSETOF(SDL_Vertex, color)},
		{NK_VERTEX_LAYOUT_END}
	};

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

	offset = (const nk_draw_index*)nk_buffer_memory_const(&ebuf);

	SDL_Rect saved_clip;
	SDL_GetRenderClipRect(renderer, &saved_clip);
	nk_bool clipping_enabled = SDL_RenderClipEnabled(renderer);

	nk_draw_foreach(cmd, ctx, &device->cmds)
	{

		if (!cmd->elem_count) continue;

		{
			SDL_Rect r;
			r.x = cmd->clip_rect.x;
			r.y = cmd->clip_rect.y;
			r.w = cmd->clip_rect.w;
			r.h = cmd->clip_rect.h;
			SDL_SetRenderClipRect(renderer, &r);
		}

		{
			const void* vertices = nk_buffer_memory_const(&vbuf);
			//SDL_RenderGeometry(renderer, (SDL_Texture*)cmd->texture.ptr, (const SDL_Vertex*)vertices, vbuf.needed / vs, (void*)offset, cmd->elem_count);
			// Needed if we change nk_draw_index to be two bytes!
			SDL_RenderGeometryRaw(renderer,
				(SDL_Texture*)cmd->texture.ptr,
				(const float*)((const nk_byte*)vertices + vp), vs,
				(const SDL_FColor*)((const nk_byte*)vertices + vc), vs,
				(const float*)((const nk_byte*)vertices + vt), vs,
				(vbuf.needed / vs),
				(void*)offset, cmd->elem_count, sizeof(nk_draw_index));

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

struct pillow_nk_sdl_t* nk_sdl_allocate(pillow_allocator* allocator, SDL_Renderer* renderer)
{
	struct nk_font_config config = nk_font_config(0);

	pillow_nk_sdl_t* nk = (pillow_nk_sdl_t*)pillow_malloc(allocator, sizeof(*nk));
	pillow_nk_device_t* device = &nk->device;
	nk_sdl_init(&nk->ctx, &device->cmds);
	struct nk_font_atlas* atlas = nk_sdl_font_stash_begin(&nk->atlas);
	struct nk_font* font = nk_font_atlas_add_default(atlas, 13.0f, &config);
	device->font_tex = nk_sdl_font_stash_end(&nk->ctx, atlas, renderer, &device->tex_null);
	nk_style_set_font(&nk->ctx, &font->handle);

	nk_set_style(&nk->ctx, nk_theme_red);
	return nk;
}

void nk_sdl_free(struct pillow_allocator* allocator, struct pillow_nk_sdl_t* nk)
{
	pillow_free(allocator, nk);
}

struct nk_context* nk_sdl(struct pillow_nk_sdl_t* nk_sdl)
{
	return &nk_sdl->ctx;
}

// NK EXTENSIONS!

void nk_update_input(struct nk_context* ctx)
{
	pillow_nk_sdl_t* nk_sdl = (pillow_nk_sdl_t*)ctx;
	memcpy(nk_sdl->prev.buttons, ctx->input.mouse.buttons, sizeof(nk_sdl->prev.buttons));
}

struct nk_mouse_button* nk_previous_buttons(struct nk_context* ctx)
{
	pillow_nk_sdl_t* nk_sdl = (pillow_nk_sdl_t*)ctx;
	return nk_sdl->prev.buttons;
}

