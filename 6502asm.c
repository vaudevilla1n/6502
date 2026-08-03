#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

enum token_type {
	T_EOF,
	T_INVALID,

	T_COMMENT,
	
	T_LPAREN,
	T_RPAREN,
	T_COMMA,
	T_NEWLINE,

	T_INSTRUCTION,
	T_REGISTER,
	T_ADDRESS,
	T_BYTE,
};

struct token_pos {
	size_t start;
	size_t end;
};

struct token {
	enum token_type	type;
	struct token_pos pos;
};

struct lexer {
	size_t srclen;
	const char *src;
	const char *curr;
	struct token token;
};

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
	fclose(fp);
	return 0;
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

		fwrite(dat, sizeof(dat[0]), len, stdout);
		fflush(stdout);
	}
}
