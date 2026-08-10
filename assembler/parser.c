#include "parser.h"

static struct stmt *parse_statement(struct lexer *l)
{
	u_unused(l);
	return 0;
}

struct stmt *parse(struct lexer *l)
{
	lexer_error(l, "testing this out");
	return parse_statement(l);
}
