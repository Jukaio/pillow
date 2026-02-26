#ifndef NUKLEAR_DEFAULT_H_INCLUDED
#define NUKLEAR_DEFAULT_H_INCLUDED

enum nk_theme
{
	nk_theme_black,
	nk_theme_white,
	nk_theme_red,
	nk_theme_blue,
	nk_theme_dark,
	nk_theme_dracula,
	nk_theme_catppuccin_latte,
	nk_theme_catppuccin_frappe,
	nk_theme_catppuccin_macchiato,
	nk_theme_catppuccin_mocha
};

// trigger on release maybe nice?
//#define NK_BUTTON_TRIGGER_ON_RELEASE
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_KEYSTATE_BASED_INPUT
#define NK_UINT_DRAW_INDEX
#include "nuklear.h"

struct nk_color* nk_set_style(struct nk_context* ctx, enum nk_theme theme);

#endif // !NUKLEAR_DEFAULT_H_INCLUDED