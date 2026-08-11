#include "parser.h"
#include <stdbool.h>

static uint16_t current_program_address;

static inline struct stmt *statement_new(enum stmt_type type, struct u_arena *arena)
{
	struct stmt *s = u_arena_alloc(arena, sizeof(*s));
	s->type = type;
	return s;
}

static inline void parser_error(struct lexer *l, const struct token *t, const char *msg)
{
	assembler_error(&l->errs, l->file, t, msg, l->arena);
}

static inline struct stmt *invalid_statement(struct lexer *l, const struct token *t, const char *msg)
{
	parser_error(l, t, msg);
	while (l->token.type != T_EOF && l->token.type != T_NEWLINE)
		lexer_next(l);
	return statement_new(STMT_INVALID, l->arena);
}

static inline bool eat(struct lexer *l, enum token_type type)
{
	bool matched = (l->token.type == type);
	if (matched)
		lexer_next(l);
	return matched;
}

static inline bool expect(struct lexer *l, enum token_type type, const char *msg)
{
	bool eaten = eat(l, type);
	if (!eaten)
		parser_error(l, &l->token, msg);
	return eaten;
}

static struct stmt *parse_statement_instruction(struct lexer *l)
{
	struct token t_ins = l->token;
	lexer_next(l);

	if (eat(l, T_NEWLINE)) {
		struct stmt *s = statement_new(STMT_INSTRUCTION, l->arena);
		s->ins.ins = t_ins;
		s->ins.addr_mode = ADDR_MODE_IMP;
		return s;
	}

	return invalid_statement(l, &t_ins, "unsupported instruction");
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
	s->label.addr = current_program_address;
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
		
	default:		return invalid_statement(l, &l->token, "erroneous token");
	}
}

struct stmt *parse(struct lexer *l)
{
	current_program_address = 0x0000;

	struct stmt *stmts = u_arena_alloc(l->arena, sizeof(*stmts));
	u_list_init(stmts);

	for (;;) {
		struct stmt *s = parse_statement(l);
		if (!s)
			break;
		u_list_append(stmts, s);
	}

	return stmts;
}
