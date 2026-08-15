#include "assembler_error.h"
#include "util.h"

struct assembler_error assembler_error_list = { 0 };

static struct u_arena allocator;

void assembler_error_list_init(void)
{
	allocator = u_arena_new(KB(64));
	u_list_init(&assembler_error_list);
}

void assembler_error_list_clear(void)
{
	u_arena_clear(&allocator);
	u_list_init(&assembler_error_list);
}

void assembler_error(const char *file, const struct token *token, const char *msg)
{
	struct assembler_error *e = u_arena_alloc(&allocator, sizeof(*e));
	e->file = file;
	e->token = *token;
	e->msg = msg;
	u_list_append(&assembler_error_list, e);
}

