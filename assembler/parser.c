/*
  stmt ::= instruction_stmt | label_definition_stmt
  
  instruction_stmt	::= INSTRUCTION instruction_operands
  instruction_operands	::= [ primary [ ',' register ] ]
  			| '(' byte_address ')' ',' register
			| '(' byte_address ',' register ')'
			| offset
			| 'A'
  primary	::= byte | byte_address | address | label
  byte		::= '#' HEX_U8
  byte_address	::= '$' HEX_U8
  address	::= '$' HEX_U16
  offset	::= '*' DEC_I16
  register	::= 'X' | 'Y'

  label_definition_stmt	::= label ':'

  label		::= ( '_' | '.' | a-z | A-Z ) [ a-z | A-Z | 0-9 | '_' ]*

 */
#include "parser.h"
#include <stdbool.h>

static struct u_arena allocator = { 0 };

static inline struct stmt *stmt_new(enum stmt_type type)
{
	struct stmt *s = u_arena_alloc(&allocator, sizeof(*s));
	s->type = type;
	return s;
}

static inline void parser_error(const struct lexer *l, const char *msg)
{
	assembler_error(l->file, &l->token, msg);
}

static inline bool next_is(struct lexer *l, enum token_type type)
{
	return (l->token.type == type);
}

static inline bool eat(struct lexer *l, enum token_type type)
{
	bool matched = next_is(l, type);
	if (matched)
		lexer_next(l);
	return matched;
}

static bool expect(struct lexer *l, enum token_type type, const char *msg)
{
	bool eaten = eat(l, type);
	if (!eaten) {
		parser_error(l, msg);
		while (l->token.type != T_EOF && l->token.type != T_NEWLINE)
			lexer_next(l);
	}
	return eaten;
}

static inline bool next_is_operand(struct lexer *l)
{
	switch (l->token.type) {
	case T_BYTE:
	case T_BYTE_ADDRESS:
	case T_ADDRESS:
	case T_LABEL:
		return true;
	case T_REGISTER:
		return (l->token.reg == REG_A);
	default:
		return false;
	}
}

static void parse_operands(struct lexer *l, struct stmt *s)
{
	bool open = eat(l, T_LPAREN);
	
	if (!next_is_operand(l)) {
		if (open) {
			s->type = STMT_INVALID;
			parser_error(l, "expected operand");
		}
		return;
	}

	s->ins.ops[s->ins.nops++] = lexer_next_token(l);

	if (eat(l, T_RPAREN))
		open = false;
	
	if (eat(l, T_COMMA)) {
		if (next_is(l, T_REGISTER) && (l->token.reg == REG_X || l->token.reg == REG_Y)) {
			s->ins.ops[s->ins.nops++] = lexer_next_token(l);
		} else {
			parser_error(l, "expected register (X or Y)");
			lexer_next(l);
			s->type = STMT_INVALID;
		}
	}

	if (open && !expect(l, T_RPAREN, "expected closing parentheses"))
		s->type = STMT_INVALID;
}

static struct stmt *parse_instruction(struct lexer *l)
{
	struct stmt *s = stmt_new(STMT_INSTRUCTION);
	s->ins.token = lexer_next_token(l);
	s->ins.nops = 0;
	parse_operands(l, s);
	
	return s;
}

static struct stmt *parse_label(struct lexer *l)
{
	struct token label = l->token;
	lexer_next(l);
	
	if (!expect(l, T_SEMICOLON, "expected semicolon after label declaration"))
		return stmt_new(STMT_INVALID);

	struct stmt *s = stmt_new(STMT_LABEL);
	s->label = label;
	return s;
}

static struct stmt *parse_stmt(struct lexer *l)
{
	while (l->token.type == T_NEWLINE)
		lexer_next(l);

	struct stmt *s = 0;
	switch (l->token.type) {
	case T_INSTRUCTION:	s = parse_instruction(l); break;
	case T_LABEL:		s = parse_label(l); break;
		
	case T_EOF:		return 0;
		
	default:
		parser_error(l, "erroneous token");
		return stmt_new(STMT_INVALID);
	}

	if (!next_is(l, T_NEWLINE) && !expect(l, T_EOF, "expected end of line"))
		s->type = STMT_INVALID;

	return s;
}

struct stmt *parse(struct lexer *l)
{
	if (!allocator.cap)
		allocator = u_arena_new(KB(64));
	else
		u_arena_clear(&allocator);
	
	struct stmt *stmts = u_arena_alloc(&allocator, sizeof(*stmts));
	u_list_init(stmts);

	for (;;) {
		struct stmt *s = parse_stmt(l);
		if (!s)
			break;
		u_list_append(stmts, s);
	}

	return stmts;
}
