#include "parser.h"
#include <stdbool.h>

static uint16_t current_program_address;

static inline struct stmt *statement_new(enum stmt_type type,
					 struct u_arena *arena)
{
	struct stmt *s = u_arena_alloc(arena, sizeof(*s));
	s->type = type;
	return s;
}

static struct stmt *invalid_statement_new(struct u_arena *arena)
{
	return statement_new(STMT_INVALID, arena);
}

static struct stmt *instruction_statement_new(const struct token *ins,
					     enum addressing_mode addr_mode,
					     const struct token *op0,
					     const struct token *op1,
					     struct u_arena *arena)
{
	struct stmt *s = u_arena_alloc(arena, sizeof(*s));
	s->ins.ins = *ins;
	s->ins.addr_mode = addr_mode;
	if (op0)
		s->ins.ops[0] = *op0;
	if (op1)
		s->ins.ops[1] = *op1;
	return s;
}

static inline void parse_error(struct lexer *l, const struct token *t,
				const char *msg)
{
	assembler_error(&l->errs, l->file, t, msg, l->arena);
}

static inline void skip_statement(struct lexer *l)
{
	while (l->token.type != T_EOF && l->token.type != T_NEWLINE)
		lexer_next(l);
}

static struct stmt *invalid_statement(struct lexer *l, const struct token *t,
				      const char *msg)
{
	parse_error(l, t, msg);
	skip_statement(l);
	return invalid_statement_new(l->arena);
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
		parse_error(l, &l->token, msg);
	return eaten;
}

static inline struct stmt *parse_accumulator(struct lexer *l,
					     const struct token *ins)
{
	struct stmt *s = 0;
	
	if (l->token.t_reg != REG_A) {
		parse_error(l, &l->token, "only the accumulator register is allowed as a sole operand");
		s = invalid_statement_new(l->arena);
	} else {
		s = instruction_statement_new(ins, ADDR_MODE_ACC, 0, 0, l->arena);
	}
	
	lexer_next(l);
	return s;
}

static inline struct stmt *parse_immediate(struct lexer *l,
					   const struct token *ins)
{
	struct token byte = l->token;
	lexer_next(l);
	return instruction_statement_new(ins, ADDR_MODE_IMM, &byte, 0, l->arena);
}

static struct stmt *parse_statement_instruction(struct lexer *l)
{
	struct token ins = l->token;
	lexer_next(l);

	struct stmt *s = 0;
	switch (l->token.type) {
	case T_EOF:
	case T_NEWLINE:	{
		s = instruction_statement_new(&ins, ADDR_MODE_IMP, 0, 0, l->arena);
	} break;
		
	case T_REGISTER: {
		s = parse_accumulator(l, &ins);
	} break;
		
	case T_BYTE: {
		s = parse_immediate(l, &ins);
	} break;
		
	default: {
		return invalid_statement(l, &ins, "unsupported instruction");
	}
	}
	
	if (!eat(l, T_NEWLINE) && !eat(l, T_EOF)) {
		parse_error(l, &l->token, "junk at end of line");
		skip_statement(l);
		s->type = STMT_INVALID;
	}

	return s;
}

static struct stmt *parse_statement_label(struct lexer *l)
{
	struct token t_label = l->token;
	lexer_next(l);
	
	if (!expect(l, T_SEMICOLON, "label declaration requires semicolon")
	    || !expect(l, T_NEWLINE, "garbage after label declaration")) {
		skip_statement(l);
		return invalid_statement_new(l->arena);
	}

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
	case T_INSTRUCTION:
		return parse_statement_instruction(l);
	case T_LABEL:
		return parse_statement_label(l);
	case T_EOF:
		return 0;
		
	default:
		return invalid_statement(l, &l->token, "erroneous token");
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
