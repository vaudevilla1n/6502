#include "assembler_error.h"

void assembler_error(struct assembler_error *err_head, const char *file, const struct token *token, const char *msg, struct u_arena *arena)
{
	struct assembler_error *e = u_arena_alloc(arena, sizeof(*e));
	e->file = file;
	e->token = *token;
	e->msg = msg;
	u_list_append(err_head, e);
}
