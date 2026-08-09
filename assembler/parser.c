#include "parser.h"

static void parser_error(struct parser *p, const char *msg)
{
	struct parse_error *e = u_arena_alloc(p->arena, sizeof(*e));
	*e = (struct parse_error) {
		.file = p->file,
		.line = p->lexer->line,
		.col = p->lexer->col,

		.msg = msg,
		.token = p->lexer->token,
	};
	u_list_head_append(&p->err, e);
}

struct parser parser_new(const char *file, struct lexer *l, struct u_arena *arena)
{
	struct parser p = {
		.file = file,
		.lexer = l,
		.arena = arena,
		.err = { 0 },
	};
	u_list_head_init(&p.err);
	return p;
}

static struct stmt *parse_statement(struct parser *p)
{
	u_unused(p);
	u_todo("parse_statement");
	return 0;
}

struct stmt *parse(struct parser *p)
{
	parser_error(p, "test error");
	return parse_statement(p);
}
