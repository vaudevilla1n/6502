/*
  assembler for 6502 assembly

  https://planetmath.org/goodhashtableprimes -> used for hash map primes
 */
#include "util.h"
#include "lexer.h"
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

	if (fseek(fp, 0, SEEK_END))
		goto cleanup_file;
	size_t fsz = ftell(fp);
	rewind(fp);
	
	char *dat = calloc(fsz, sizeof(dat[0]));
	if (!dat)
		goto cleanup_file;
	size_t len = fread(dat, sizeof(dat[0]), fsz, fp);

	fclose(fp);
	*datlen = len;
	return dat;

cleanup_file:

	return 0;
}

static inline void token_print(const struct token *t, const char *src)
{
	printf("%zu,%zu %s ", t->pos.start + 1, t->pos.end + 1,
	       token_type_name[t->type]);

	if (t->type == T_NEWLINE) {
		printf("'\\n'");
	} else {
		size_t lexeme_len = t->pos.end - t->pos.start;
		const char *lexeme = src + t->pos.start;
		printf("'%.*s'", (int)lexeme_len, lexeme);
	}

	switch (t->type) {
	case T_BYTE:		printf(" (%hhx)", t->t_byte); break;
	case T_BYTE_ADDRESS:	printf(" (%hhx)", t->t_byte_address); break;
	case T_ADDRESS:		printf(" (%hx)", t->t_address); break;
	case T_REGISTER: {
		switch (t->t_register) {
		case REG_A:	printf(" (A)"); break;
		case REG_X:	printf(" (X)"); break;
		case REG_Y:	printf(" (Y)"); break;
		default: u_unreachable("token_print");
		}
	} break;
	case T_INSTRUCTION:	printf(" (%s)", instruction_to_string[t->t_instruction]); break;
	default:		break;
	}

	printf("\n");
}

int main(int argc, char **argv)
{
	for (int i = 1; i < argc; i++) {
		const char *path = argv[i];

		size_t len = 0;
		char *dat = read_file(path, &len);
		if (!dat) {
			perror(path);
			continue;
		}

		struct lexer l = lexer_new(dat, len);
		for (;;) {
			token_print(&l.token, l.src);
			if (l.token.type == T_EOF)
				break;
			lexer_next(&l);
		}
	}
}
