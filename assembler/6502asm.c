/*
  assembler for 6502 assembly

  https://planetmath.org/goodhashtableprimes -> used for hash map primes
 */

#define UTIL_H_IMPL
#include "util.h"
#include "lexer.h"
#include "parser.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

static char *read_file(const char *path, size_t *datlen)
{
	FILE *fp = fopen(path, "r");
	if (!fp)
		return 0;

	if (fseek(fp, 0, SEEK_END)) {
		fclose(fp);
		return 0;
	}

	size_t fsz = ftell(fp);
	rewind(fp);
	
	char *dat = u_calloc(fsz, sizeof(dat[0]));
	*datlen = fread(dat, sizeof(dat[0]), fsz, fp);

	fclose(fp);
	return dat;
}

static void dump_tokens(const char *file, char *src, size_t srclen, struct u_arena *arena)
{
	struct lexer l;
	lexer_init(&l, file, src, srclen, arena);
	
	for (;;) {
		token_print(&l.token);
		if (l.token.type == T_EOF)
			break;
		lexer_next(&l);
	}

	u_arena_free(arena);
}

int main(int argc, char **argv)
{
	struct u_arena arena = u_arena_new(u_KiB(64));

	lexer_instruction_map_init();
	
	for (int i = 1; i < argc; i++) {
		const char *path = argv[i];

		size_t len = 0;
		char *src = read_file(path, &len);
		if (!src) {
			perror(path);
			continue;
		}

		// testing
		{
			dump_tokens(path, src, len, &arena);
		}

		struct lexer l;
		lexer_init(&l, path, src, len, &arena);
		
		struct stmt *stmts = parse(&l);
		printf("%p\n", (void *)stmts);

		u_list_for_each(&l.errs, e)
			printf("%s:%zu:%zu:error %s: '%.*s'\n",
			       e->file, e->token.line, e->token.col, e->msg, (int)e->token.len, e->token.text);
		
		u_arena_free(&arena);
	}
}
