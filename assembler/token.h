#pragma once

#include "6502_constants.h"
#include <stdint.h>
#include <stddef.h>

enum token_type : uint8_t {
	T_EOF,
	T_INVALID,

	T_COMMENT,
	
	T_LPAREN, T_RPAREN,
	T_COMMA, T_NEWLINE,
	T_SEMICOLON,

	T_INSTRUCTION, T_REGISTER, T_LABEL,
		
	T_ADDRESS, T_BYTE_ADDRESS, T_BYTE,

	TOKEN_COUNT,
};

extern const char *token_type_name[TOKEN_COUNT];

struct token {
	enum token_type	type;

	size_t len;
	const char *text;
	
	size_t col;
	size_t line;
	
	union {
		uint8_t byte;
		uint8_t byte_addr;
		uint16_t addr;
		enum instruction ins;
		enum machine_register reg;
	};
};

void token_print(const struct token *t);
