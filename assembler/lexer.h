#pragma once

#include "6502_constants.h"
#include <stddef.h>

enum token_type {
	T_EOF,
	T_INVALID,

	T_COMMENT,
	
	T_LPAREN, T_RPAREN,
	T_COMMA, T_NEWLINE,

	T_INSTRUCTION, T_REGISTER,
	T_ADDRESS, T_BYTE_ADDRESS,
	T_BYTE,

	TOKEN_COUNT,
};

extern const char *token_type_name[TOKEN_COUNT];

struct token_pos {
	size_t start;
	size_t end;
};

struct token {
	enum token_type	type;
	struct token_pos pos;
	union {
		uint8_t t_byte;
		uint8_t t_byte_address;
		uint16_t t_address;
		enum instruction t_instruction;
		enum machine_register t_register;
	};
};

struct lexer {
	size_t srclen;
	const char *src;
	const char *curr;
	struct token token;
};

struct lexer lexer_new(const char *src, size_t srclen);
void lexer_next(struct lexer *l);
