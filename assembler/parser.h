#pragma once

#include "6502_constants.h"
#include "assembler_error.h"
#include "lexer.h"
#include "util.h"
#include <stdint.h>

#define STMT_MAX_OPERANDS	2

enum stmt_type {
	STMT_INVALID,

	// STMT_BLANK <- i was tired, trust
	STMT_LABEL,
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

		struct {
			uint16_t address;
			struct token token;
		} label;
	};

	struct stmt *next;
	struct stmt *prev;
};

struct stmt *parse(struct lexer *l);
