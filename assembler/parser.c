#include "parser.h"
#include <stdbool.h>

static uint16_t current_program_address;

static inline struct stmt *statement_new(enum stmt_type type, struct u_arena *arena)
{
	struct stmt *s = u_arena_alloc(arena, sizeof(*s));
	s->type = type;
	return s;
}

static struct stmt *parse_statement_instruction(struct lexer *l)
{
	u_unused(l);
	u_todo("parse_statement_instruction");
}

static inline bool expect(struct lexer *l, enum token_type type, const char *msg)
{
	bool matched = (l->token.type == type);
	if (matched)
		lexer_next(l);
	else
		lexer_error(l, msg);
	return matched;
}

static struct stmt *parse_statement_label(struct lexer *l)
{
	struct token t_label = l->token;
	lexer_next(l);
	
	if (!expect(l, T_SEMICOLON, "label declaration requires semicolon")
	    || !expect(l, T_NEWLINE, "garbage after label declaration"))
		return statement_new(STMT_INVALID, l->arena);

	struct stmt *s = statement_new(STMT_LABEL, l->arena);
	s->label.token = t_label;
	s->label.address = current_program_address;
	return s;
}

static struct stmt *parse_statement(struct lexer *l)
{
	while (l->token.type == T_NEWLINE)
		lexer_next(l);
	
	switch (l->token.type) {
	case T_INSTRUCTION:	return parse_statement_instruction(l);
	case T_LABEL:		return parse_statement_label(l);
		
	case T_EOF:		return 0;
		
	default:		lexer_error(l, "erroneous token"); return statement_new(STMT_INVALID, l->arena);
	}
}

struct stmt *parse(struct lexer *l)
{
	current_program_address = 0x0000;

	struct stmt *stmts = u_arena_alloc(l->arena, sizeof(*stmts));
	u_list_head_init(stmts);

	for (;;) {
		struct stmt *s = parse_statement(l);
		if (!s)
			break;
		u_list_head_append(stmts, s);
	}

	return stmts;
}
