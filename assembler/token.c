#include "token.h"
#include "util.h"
#include <stdio.h>

const char *token_type_name[TOKEN_COUNT] = {
	"T_EOF",
	"T_INVALID",

	"T_COMMENT",
	
	"T_LPAREN", "T_RPAREN",
	"T_COMMA", "T_NEWLINE",
	"T_SEMICOLON",

	"T_INSTRUCTION", "T_REGISTER", "T_LABEL",
	
	"T_ADDRESS", "T_BYTE_ADDRESS", "T_BYTE",
};

void token_print(const struct token *t)
{
	printf("%zu,%zu %s ", t->line, t->col,
	       token_type_name[t->type]);

	if (t->type == T_NEWLINE)
		printf("'\\n'");
	else
		printf("'%.*s'", (int)t->len, t->text);
	
	switch (t->type) {
	case T_BYTE:		printf(" (%hhx)", t->byte); break;
	case T_BYTE_ADDRESS:	printf(" (%hhx)", t->byte_addr); break;
	case T_ADDRESS:		printf(" (%hx)", t->addr); break;
	case T_REGISTER: {
		switch (t->reg) {
		case REG_A:	printf(" (A)"); break;
		case REG_X:	printf(" (X)"); break;
		case REG_Y:	printf(" (Y)"); break;
		default: u_unreachable("token_print");
		}
	} break;
	case T_INSTRUCTION:	printf(" (%s)", instruction_to_string[t->ins]); break;
	default:		break;
	}

	printf("\n");
}
