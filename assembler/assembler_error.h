#pragma once

#include "util.h"
#include "token.h"

struct assembler_error {
	const char *file;
	const char *msg;
	struct token token;

	struct assembler_error *next;
	struct assembler_error *prev;
};

extern struct assembler_error assembler_error_list;

void assembler_error_list_init(void);
void assembler_error_list_clear(void);

void assembler_error(const char *file, const struct token *token, const char *msg);
