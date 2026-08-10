#include "lexer.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

const char *token_type_name[TOKEN_COUNT] = {
	"T_EOF",
	"T_INVALID",

	"T_COMMENT",
	
	"T_LPAREN", "T_RPAREN",
	"T_COMMA", "T_NEWLINE",

	"T_INSTRUCTION", "T_REGISTER",
	"T_ADDRESS", "T_BYTE_ADDRESS",
	"T_BYTE",
};

struct instruction_map_pair {
	size_t keylen;
	const char *key;
	enum instruction val;
};

#define INSTRUCTION_MAP_CAPACITY	(INSTRUCTION_COUNT * 4)

struct instruction_map {
	size_t len;
	size_t cap;
	struct instruction_map_pair index[INSTRUCTION_MAP_CAPACITY];
};

static struct instruction_map instruction_map = { 0 };

#define INSTRUCTION_NAME_LEN		3
#define INSTRUCTION_MAP_HASH_PRIME	50331653

static inline char uppercase(char c)
{
	return (c & ~32);
}

static inline uint64_t instruction_map_hash(const char *key)
{
	uint64_t h = (uppercase(key[0]) << 16)
		| (uppercase(key[1]) << 8)
		| uppercase(key[2]);
	h = INSTRUCTION_MAP_HASH_PRIME ^ ((h << 24) | h);
	h >>= 10;
	return h;
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

void lexer_instruction_map_init(void)
{
	instruction_map.cap = INSTRUCTION_MAP_CAPACITY;
	instruction_map.len = 0;

	for (size_t i = INSTRUCTION_INDEX_FIRST; i < INSTRUCTION_COUNT; i++) {
		const char *key = instruction_to_string[i];
		instruction_map_insert(key, i);
	}
}

static int instruction_map_find(const char *key)
{
	uint64_t idx = instruction_map_hash(key) % instruction_map.cap;
	for (size_t i = 0; i < instruction_map.cap; i++) {
		const char *map_key = instruction_map.index[idx].key;
		if (map_key && u_streq(map_key, key, INSTRUCTION_NAME_LEN))
			return instruction_map.index[idx].val;
		idx = (idx + 1) % instruction_map.cap;
	}

	return -1;
}

void lexer_init(struct lexer *l, const char *file, const char *src, size_t srclen, struct u_arena *arena)
{
	*l = (struct lexer) {
		.arena = arena,

		.srclen = srclen,
		.src = src,
		.file = file,
		
		.curr = src,
		.linepos = 0,
		.col = 1,
		.line = 1,
		
		.token = { .type = T_INVALID },

		.err = { 0 },
	};
	
	u_list_head_init(&l->err);
	lexer_next(l);
}

const char *lexer_token_text(const struct lexer *l, size_t *out_len)
{
	*out_len = l->token.end - l->token.start;
	return l->src + l->token.start;
}

static inline size_t lexer_pos(const struct lexer *l)
{
	return l->curr - l->src;
}

void lexer_error(struct lexer *l, const char *msg)
{
	l->token.type = T_INVALID;
	l->token.end = lexer_pos(l);
	
	struct assembler_error e = {
		.file = l->file,
		.line = l->line,
		.col = l->col,
		.msg = msg,
	};
	e.src = lexer_token_text(l, &e.srclen);

	assembler_error_append(&l->err, &e, l->arena);
}

static inline bool lexer_eof(const struct lexer *l)
{
	return l->curr >= (l->src + l->srclen);
}

static inline void lex_newline(struct lexer *l)
{
	l->token.type = T_NEWLINE;
	l->line++;
	l->linepos = lexer_pos(l) - 1;
}

static void lex_comment(struct lexer *l)
{
	l->token.type = T_COMMENT;
	
	while (!lexer_eof(l) && *l->curr != '\n')
		l->curr++;

	if (*l->curr == '\n') {
		l->linepos = lexer_pos(l);
		l->line++;
		l->curr++;
	}
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
		
	default: lexer_error(l, "invalid number literal"); break;
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
		lexer_error(l, "invalid byte literal");
	}
}

static inline bool whitespace(char c)
{
	return (c <= 0x20);
}

static inline bool alpha(char c)
{
	return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z');
}

static inline bool digit(char c)
{
	return ('0' <= c && c <= '9');
}

static inline bool identifier(char c)
{
	return alpha(c) || digit(c) || (c == '_');
}

static inline int register_find(char c)
{
	switch (c) {
	case 'A':
		return REG_A;
	case 'X':
		return REG_X;
	case 'Y':
		return REG_Y;
	default:
		return -1;
	}
}

static void lex_identifier(struct lexer *l)
{
	bool invalid_chars = false;
	
	size_t start = lexer_pos(l) - 1;
	while (!lexer_eof(l) && !whitespace(*l->curr))
	{
		if (*l->curr == ':') {
			l->token.type = T_LABEL;
			l->curr++;
			break;
		}
		
		if (!identifier(*l->curr))
			invalid_chars = true;
		
		l->curr++;
	}

	if (invalid_chars)
		goto invalid_identifier;
	
	size_t len = lexer_pos(l) - start;

	if (l->token.type == T_LABEL) {
		l->token.label.id = l->src + start;
		l->token.label.len = len - 1;
	} else if (len == 1) {
		char id = l->curr[-1];
		int reg = register_find(id);
		if (reg != -1) {
			l->token.type = T_REGISTER;
			l->token.t_register = reg;
		} else {
			goto invalid_identifier;
		}
	} else if (len == 3) {
		const char *id = l->src + start;
		int ins = instruction_map_find(id);
		if (ins != -1) {
			l->token.type = T_INSTRUCTION;
			l->token.t_instruction = ins;
		} else {
			goto invalid_identifier;
		}
	} else {
		goto invalid_identifier;
	}

	return;

invalid_identifier:
	lexer_error(l, "invalid identifier");
}

static inline bool skippable_whitespace(char c)
{
	return (c <= 0x20) && c != '\n';
}

void lexer_next(struct lexer *l)
{
	if (l->token.type == T_EOF)
		return;

	while (!lexer_eof(l) && skippable_whitespace(*l->curr))
		l->curr++;

	if (lexer_eof(l)) {
		l->token.type = T_EOF;
		l->token.start = l->srclen;
		l->token.end = l->srclen;
		return;
	}

	l->token.start = lexer_pos(l);
	l->col = l->token.start - l->linepos;
	
	char c = *l->curr++;
	switch (c) {
	case '(':	l->token.type = T_LPAREN; break;
	case ')':	l->token.type = T_RPAREN; break;
	case ',':	l->token.type = T_COMMA; break;
		
	case '\n':	lex_newline(l); break;

	case ';':	lex_comment(l); break;
		
	case '$':	lex_address(l); break;
	case '#':	lex_byte(l); break;
		
	default: {
		if (alpha(c) || c == '.' || c == '_')
			lex_identifier(l);
		else
			lexer_error(l, "unknown token");
	} break;
	}
	
	l->token.end = lexer_pos(l);
}

void lexer_token_print(const struct lexer *l)
{
	const struct token *t = &l->token;
	
	printf("%zu,%zu %s ", t->start + 1, t->end + 1,
	       token_type_name[t->type]);

	if (t->type == T_NEWLINE) {
		printf("'\\n'");
	} else {
		size_t lexeme_len = t->end - t->start;
		const char *lexeme = l->src + t->start;
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
