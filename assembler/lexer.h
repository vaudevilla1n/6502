#pragma once

#include "6502_constants.h"
#include "util.h"
#include "assembler_error.h"
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
	size_t start;
	size_t end;
	union {
		uint8_t t_byte;
		uint8_t t_byte_address;
		uint16_t t_address;
		enum instruction t_instruction;
		enum machine_register t_register;

		struct {
			size_t len;
			const char *id;
		} label;
	};
};

struct lexer {
	struct u_arena *arena;
	
	size_t srclen;
	const char *src;
	const char *file;
	
	const char *curr;
	
	size_t linepos;
	size_t col;
	size_t line;

	struct token token;
	
	struct assembler_error errs;
};

void lexer_instruction_map_init(void);

void lexer_init(struct lexer *l, const char *file, const char *src, size_t srclen, struct u_arena *arena);
const char *lexer_token_text(const struct lexer *l, size_t *out_len);
void lexer_error(struct lexer *l, const char *msg);
void lexer_next(struct lexer *l);

void lexer_token_print(const struct lexer *l);
