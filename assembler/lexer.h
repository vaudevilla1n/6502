#pragma once

#include "token.h"
#include "6502_constants.h"
#include "util.h"
#include "assembler_error.h"
#include <stddef.h>

struct lexer {
	size_t srclen;
	const char *src;
	const char *file;
	
	const char *curr;
	
	size_t line;
	size_t linepos;

	struct token token;
};

void lexer_instruction_map_init(void);

struct lexer lexer_new(const char *file, const char *src, size_t srclen);
void lexer_next(struct lexer *l);
struct token lexer_next_token(struct lexer *l);
