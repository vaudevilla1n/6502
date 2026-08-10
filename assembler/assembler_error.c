#include "assembler_error.h"

void assembler_error_append(struct assembler_error *err_head, struct assembler_error *e, struct u_arena *arena)
{
	struct assembler_error *new = u_arena_alloc(arena, sizeof(*new));
	*new = *e;
	u_list_head_append(err_head, new);
}
