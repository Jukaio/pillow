#ifndef PILLOW_CASE_NUKLEAR_H_INCLUDED
#define PILLOW_CASE_NUKLEAR_H_INCLUDED

#include <inttypes.h>

struct pillow_nk_sdl_t;
struct pillow_allocator;

union SDL_Event;
struct SDL_Renderer;

struct nk_context;
struct nk_mouse_button;

int nk_sdl_handle_event(struct pillow_nk_sdl_t *nk_sdl, union SDL_Event *evt);
void nk_sdl_draw(struct pillow_nk_sdl_t *nk_sdl, struct SDL_Renderer *renderer);

struct nk_context *nk(struct pillow_nk_sdl_t *nk_sdl);
struct pillow_nk_sdl_t *nk_sdl_allocate(struct pillow_allocator *allocator, struct SDL_Renderer *renderer);
void nk_sdl_free(struct pillow_allocator *allocator, struct pillow_nk_sdl_t *nk);

int nk_dock_begin(struct nk_context* ctx, float x, float y, float w, float h);
void nk_dock_end(struct nk_context* ctx);

int nk_dock_popup(struct nk_context *ctx, float x, float y, float w, float h);

// Extensions - Only works with pillow_nk_sdl_t
// void nk_update_input(struct nk_context* ctx);
// struct nk_mouse_button* nk_previous_buttons(struct nk_context* ctx);

#endif // !PILLOW_CASE_NUKLEAR_H_INCLUDED
