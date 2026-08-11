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

void assembler_error(struct assembler_error *err_head, const char *file, const struct token *token, const char *msg, struct u_arena *arena);
