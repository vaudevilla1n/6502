/*
  assembler for 6502 assembly

  https://planetmath.org/goodhashtableprimes -> used for hash map primes
 */
#include "6502_constants.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define unreachable(f) \
	do { fprintf(stderr, "unreachable: %s\n", f); abort(); } while (0)

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
	union {
		uint8_t t_byte;
		uint8_t t_byte_address;
		uint16_t t_address;
		enum instruction t_instruction;
		enum machine_register t_register;
	};
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

#define INSTRUCTION_MAP_CAPACITY	(INSTRUCTION_COUNT * 3/2)

struct instruction_map {
	size_t len;
	size_t cap;
	struct instruction_map_pair index[INSTRUCTION_MAP_CAPACITY];
};

static struct instruction_map instruction_map = { 0 };

#define INSTRUCTION_NAME_LEN		3
#define INSTRUCTION_MAP_HASH_PRIME	50331653

static void instruction_map_insert(const char *key, enum instruction val);
static void instruction_map_init(void);
static int instruction_map_find(const char *key);

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
		default: unreachable("token_print");
		}
	} break;
	case T_INSTRUCTION:	printf(" (%s)", instruction_name_table[t->t_instruction]); break;
	default:		break;
	}

	printf("\n");
}

int main(int argc, char **argv)
{
	instruction_map_init();
	
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

static inline uint64_t instruction_map_hash(const char *key)
{
	uint64_t v = (key[0] << 16) | (key[1] << 8) | (key[2]);
	return INSTRUCTION_MAP_HASH_PRIME ^ v;
}

static void instruction_map_insert(const char *key, enum instruction val)
{
	size_t idx = instruction_map_hash(key) % instruction_map.cap;
	for (size_t i = 0; i < instruction_map.cap; i++) {
		if (!instruction_map.index[idx].key) {
			instruction_map.index[idx].key = key;
			instruction_map.index[idx].val = val;
			instruction_map.len++;
			return;
		}
		idx = (idx + 1) % instruction_map.cap;
	}

	fprintf(stderr, "instruction identifier map at max capacity\n");
	exit(1);
}

static void instruction_map_init(void)
{
	instruction_map.cap = INSTRUCTION_MAP_CAPACITY;
	instruction_map.len = 0;

	for (size_t i = INSTRUCTION_INDEX_FIRST; i < INSTRUCTION_COUNT; i++) {
		const char *key = instruction_name_table[i];
		instruction_map_insert(key, i);
	}
}

static int instruction_map_find(const char *key)
{
	uint64_t idx = instruction_map_hash(key) % instruction_map.cap;
	for (size_t i = 0; i < instruction_map.cap; i++) {
		const char *map_key = instruction_map.index[idx].key;
		if (map_key && !strncasecmp(map_key, key, INSTRUCTION_NAME_LEN))
			return instruction_map.index[idx].val;
		idx = (idx + 1) % instruction_map.cap;
	}

	return -1;
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
	size_t start = lexer_pos(l);
	
	size_t digits = 0;
	while (!lexer_eof(l) && hexdigit(*l->curr)) {
		digits++;
		l->curr++;
	}

	switch (digits) {
	case 2:	{
		uint32_t val = strtoul(l->src + start, 0, 16);
		l->token.type = T_BYTE_ADDRESS;
		l->token.t_byte_address = val;
	} break;
		
	case 4: {
		uint32_t val = strtoul(l->src + start, 0, 16);
		l->token.type = T_ADDRESS;
		l->token.t_address = val;
	} break;
		
	default: l->token.type = T_INVALID; break;
	}
}

static void lex_byte(struct lexer *l)
{
	size_t start = lexer_pos(l);
	
	size_t digits = 0;
	while (!lexer_eof(l) && hexdigit(*l->curr)) {
		digits++;
		l->curr++;
	}
	
	if (digits == 2) {
		uint32_t val = strtoul(l->src + start, 0, 16);
		l->token.type = T_BYTE;
		l->token.t_byte = val;
	} else {
		l->token.type = T_INVALID;
	}
}

static inline bool identifier(char c)
{
	return (c == '_') || ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z');
}

static inline bool whitespace(char c)
{
	return (c <= 0x20);
}

static inline int register_find(char c)
{
	switch (c) {
	case 'a': case 'A':
		return REG_A;
	case 'x': case 'X':
		return REG_X;
	case 'y': case 'Y':
		return REG_Y;
	default:
		return -1;
	}
}

static void lex_identifier(struct lexer *l)
{
	l->token.type = T_INVALID;
	
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
		char id = l->curr[-1];
		int reg = register_find(id);
		if (reg != -1) {
			l->token.type = T_REGISTER;
			l->token.t_register = reg;
		}
	} else if (len == 3) {
		const char *id = l->src + start;
		int ins = instruction_map_find(id);
		if (ins != -1) {
			l->token.type = T_INSTRUCTION;
			l->token.t_instruction = ins;
		}
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
