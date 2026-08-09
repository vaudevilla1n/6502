#pragma once

#include "6502_constants.h"
#include "lexer.h"
#include "util.h"

struct parse_error {
	const char *file;
	size_t col;
	size_t line;
	
	const char *msg;
	struct token token;

	struct parse_error *next;
	struct parse_error *prev;
};

struct parser {
	struct u_arena *arena;
	
	struct lexer *lexer;
	
	const char *file;
	struct parse_error err;
};

#define STMT_MAX_OPERANDS	2

enum stmt_type {
	STMT_INVALID,
	
	STMT_BLANK,
	STMT_INSTRUCTION,
};

struct stmt {
	enum stmt_type type;
	// left as union because i will most likely be parsing more statements
	union {
		struct {
			enum addressing_mode addr_mode;
			struct token ops[STMT_MAX_OPERANDS];
		} instruction;
	};

	struct stmt *next;
	struct stmt *prev;
};

struct parser parser_new(const char *file, struct lexer *l, struct u_arena *arena);

struct stmt *parse(struct parser *p);
