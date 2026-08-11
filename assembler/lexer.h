#pragma once

#include "token.h"
#include "6502_constants.h"
#include "util.h"
#include "assembler_error.h"
#include <stddef.h>

struct lexer {
	struct u_arena *arena;
	
	size_t srclen;
	const char *src;
	const char *file;
	
	const char *curr;
	
	size_t line;
	size_t linepos;

	struct token token;
	
	struct assembler_error errs;
};

void lexer_instruction_map_init(void);

void lexer_init(struct lexer *l, const char *file, const char *src, size_t srclen, struct u_arena *arena);
void lexer_next(struct lexer *l);
