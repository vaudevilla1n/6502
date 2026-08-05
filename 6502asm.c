/*
  assembler for 6502 assembly

  https://planetmath.org/goodhashtableprimes -> used for hash map primes
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

enum token_type {
	T_EOF,
	T_INVALID,

	T_COMMENT,
	
	T_LPAREN, T_RPAREN,
	T_COMMA, T_NEWLINE,

	T_INSTRUCTION, T_REGISTER,
	T_ADDRESS, T_BYTE_ADDRESS,
	T_BYTE,
};

static const char *token_type_name[] = {
	"T_EOF",
	"T_INVALID",

	"T_COMMENT",
	
	"T_LPAREN", "T_RPAREN",
	"T_COMMA", "T_NEWLINE",

	"T_INSTRUCTION", "T_REGISTER",
	"T_ADDRESS", "T_BYTE_ADDRESS",
	"T_BYTE",
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

static struct lexer lexer_new(const char *src, size_t srclen);
static void lexer_next(struct lexer *l);

struct instruction_map_pair {
	size_t keylen;
	const char *key;
	enum instruction val;
};

#define INSTRUCTION_MAP_CAPACITIY	(INSTRUCTION_COUNT * 3/2)

struct instruction_map {
	size_t len;
	size_t cap;
	struct instruction_map_pair index[INSTRUCTION_MAP_CAPACITY];
};

static struct instruction_map instruction_identifier_map = { 0 };

#define INSTRUCTION_NAME_LEN		3
#define INSTRUCTION_MAP_HASH_PRIME	50331653

static void instruction_map_insert(struct instruction_map *m, const char *key
				   enum instruction val);
static void instruction_map_init(struct instruction_map *m);
static enum instruction instruction_map_find(struct instruction_map *m,
					     const char *key);

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

static inline void token_print(const struct token *t, const char *src)
{
	printf("%zu,%zu \t %s    ", t->pos.start + 1, t->pos.end + 1,
	       token_type_name[t->type]);

	if (t->type == T_NEWLINE) {
		printf("'\\n'");
	} else {
		size_t lexeme_len = t->pos.end - t->pos.start;
		const char *lexeme = src + t->pos.start;
		printf("'%.*s'", (int)lexeme_len, lexeme);
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

static inline uint64_t_t instruction_map_hash(const char *key)
{
	uint64_t v = (key[0] << 16) | (key[1] << 8) | (key[2]);
	return INSTRUCTION_MAP_HASH_PRIME ^ v;
}

static void instruction_map_insert(struct instruction_map *m, const char *key,
				   enum instruction val)
{
	size_t idx = instruction_map_hash(key) % m->cap;
	for (size_t i = 0; i < m->cap; i++) {
		size_t j = (idx + 1) % m->cap;
		
		if (!m->index[j].key) {
			m->index[j].key = key;
			m->index[j].val = val;
			return;
		}
	}

	fprintf(stderr, "instruction identifier map at max capacity\n");
	exit(1);
}

static void instruction_map_init(struct instruction_map *m)
{
	m->cap = INSTRUCTION_MAP_CAPACITY;
	m->len = 0;

	for (size_t i = INSTRUCTION_ENUM_START; i < INSTRUCTION_COUNT; i++) {
		const char *key = instruction_name_table[i];
		instruction_map_insert(m, key, i);
	}
}

static enum instruction instruction_map_find(struct instruction_map *m,
					     const char *key)
{
}

static struct lexer lexer_new(const char *src, size_t srclen)
{
	struct lexer l = {
		.src = src,
		.srclen = srclen,
		.curr = src,
		.token = {
			.type = T_INVALID,
			.pos = { 0 },
		},
	};
	lexer_next(&l);
	return l;
}

static inline size_t lexer_pos(const struct lexer *l)
{
	return l->curr - l->src;
}

static inline bool lexer_eof(const struct lexer *l)
{
	return l->curr >= (l->src + l->srclen);
}

static void lex_comment(struct lexer *l)
{
	l->token.type = T_COMMENT;
	
	while (!lexer_eof(l) && *l->curr != '\n')
		l->curr++;

	if (*l->curr == '\n')
		l->curr++;
}

static inline bool hexdigit(char c)
{
	return ('0' <= c && c <= '9')
		|| ('a' <= c && c <= 'f')
		|| ('A' <= c && c <= 'F');
}

static void lex_address(struct lexer *l)
{
	size_t digits = 0;
	while (!lexer_eof(l) && hexdigit(*l->curr)) {
		digits++;
		l->curr++;
	}
	
	switch (digits) {
	case 2:	 l->token.type = T_BYTE_ADDRESS; break;
	case 4:  l->token.type = T_ADDRESS; break;
	default: l->token.type = T_INVALID; break;
	}
}

static void lex_byte(struct lexer *l)
{
	size_t digits = 0;
	while (!lexer_eof(l) && hexdigit(*l->curr)) {
		digits++;
		l->curr++;
	}

	if (digits == 2)
		l->token.type = T_BYTE;
	else
		l->token.type = T_INVALID;
}

static inline bool identifier(char c)
{
	return (c == '_') || ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z');
}

static inline bool whitespace(char c)
{
	return (c <= 0x20);
}

static inline bool register_identifier(char c)
{
	switch (c) {
	case 'a': case 'A':
	case 'x': case 'X':
	case 'y': case 'Y':
		return true;
	default:
		return false;
	}
}

static enum instruction lookup_instruction_id(const char *id, size_t len)
{
	
}

static void lex_identifier(struct lexer *l)
{
	size_t start = lexer_pos(l) - 1;
	while (!lexer_eof(l) && !whitespace(*l->curr))
	{
		if (!identifier(*l->curr)) {
			l->token.type = T_INVALID;
			break;
		}
		l->curr++;
	}
	size_t len = lexer_pos(l) - start;

	if (len == 1) {
		char reg = l->curr[-1];
		l->token.type = (register_identifier(reg)) ? T_REGISTER
			: T_INVALID;
	} else {
		enum instruction ins = lookup_instruction_id(l->src + start,
							     len);
		l->token.type = (ins != INS_INVALID) ? T_INSTRUCTION
			: T_INVALID;
	}
}

static inline bool skippable_whitespace(char c)
{
	return (c <= 0x20) && c != '\n';
}

static void lexer_next(struct lexer *l)
{
	if (l->token.type == T_EOF)
		return;

	while (!lexer_eof(l) && skippable_whitespace(*l->curr))
		l->curr++;

	if (lexer_eof(l)) {
		l->token.type = T_EOF;
		l->token.pos.start = l->srclen;
		l->token.pos.end = l->srclen;
		return;
	}

	l->token.pos.start = lexer_pos(l);
	char c = *l->curr++;
	switch (c) {
	case '(':	l->token.type = T_LPAREN; break;
	case ')':	l->token.type = T_RPAREN; break;
	case ',':	l->token.type = T_COMMA; break;
	case '\n':	l->token.type = T_NEWLINE; break;

	case ';':	lex_comment(l); break;
		
	case '$':	lex_address(l); break;
	case '#':	lex_byte(l); break;
		
	default: {
		if (identifier(c))
			lex_identifier(l);
		else
			l->token.type = T_INVALID;
	} break;
	}
	l->token.pos.end = lexer_pos(l);
}
