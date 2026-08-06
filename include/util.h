#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define u_unreachable(f) \
	do { fprintf(stderr, "unreachable: %s\n", f); abort(); } while (0)

#define u_str_case_eq(s, t, n)	(!strncasecmp((s), (t), (n)))
