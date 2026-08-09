/*
  assembler for 6502 assembly

  https://planetmath.org/goodhashtableprimes -> used for hash map primes
  https://www.nesdev.org/obelisk-6502-guide/addressing.html -> addressing modes
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

static void dump_tokens(char *src, size_t srclen)
{
	struct lexer l = lexer_new(src, srclen);
	for (;;) {
		lexer_token_print(&l);
		if (l.token.type == T_EOF)
			break;
		lexer_next(&l);
	}
}

int main(int argc, char **argv)
{
	struct u_arena parsing_arena = u_arena_new(u_KiB(64));
	
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
			dump_tokens(src, len);
		}

		struct lexer l = lexer_new(src, len);
		struct parser p = parser_new(path, &l, &parsing_arena);
		struct stmt *stmts = parse(&p);
		
		printf("%p\n", (void *)stmts);
	}
}
