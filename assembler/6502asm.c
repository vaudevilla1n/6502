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

int main(int argc, char **argv)
{
	assembler_error_list_init();
	lexer_instruction_map_init();
	
	for (int i = 1; i < argc; i++) {
		const char *path = argv[i];

		size_t len = 0;
		char *src = read_file(path, &len);
		if (!src) {
			perror(path);
			continue;
		}

		struct lexer l = lexer_new(path, src, len);
		
		struct stmt *stmts = parse(&l);

		u_list_for_each(&assembler_error_list, e) { 
			printf("%s:%zu:%zu:error %s: '%.*s'\n", e->file, e->token.line, e->token.col, e->msg, (int)e->token.len, e->token.text);
		}

		u_list_for_each(stmts, stmt) {
			switch (stmt->type) {
			case STMT_INVALID:	printf("invalid\n"); break;
			case STMT_LABEL:	printf("%.*s (%hx)\n", (int)stmt->label.token.len, stmt->label.token.text, stmt->label.addr); break;
			case STMT_INSTRUCTION:	printf("%.*s\n", (int)stmt->ins.type.len, stmt->ins.type.text); break;
			default: u_unreachable("for_each stmt");
			}
		}

		assembler_error_list_clear();
	}
}
