#pragma once

#include "util.h"

struct assembler_error {
	const char *file;
	size_t col;
	size_t line;
	
	const char *msg;

	size_t srclen;
	const char *src;

	struct assembler_error *next;
	struct assembler_error *prev;
};

void assembler_error_append(struct assembler_error *err_head, struct assembler_error *e, struct u_arena *arena);
