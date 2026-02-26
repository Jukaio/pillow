#ifndef NUKLEAR_DEMO_H_INCLUDED
#define NUKLEAR_DEMO_H_INCLUDED

struct nk_context;

void nk_demo_calculator(struct nk_context* ctx);

int nk_demo_overview(struct nk_context* ctx);
int nk_node_editor(struct nk_context* ctx);

void nk_demo_canvas(struct nk_context* ctx);

#endif // !NUKLEAR_DEMO_H_INCLUDED
