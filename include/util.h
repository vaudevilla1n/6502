#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define u_unreachable(f) \
	do { fprintf(stderr, "unreachable: %s\n", f); abort(); } while (0)

#define u_streq(s, t, n)	(!strncasecmp((s), (t), (n)))

#define u_unused(x)	(void)(x)
#define u_todo(func)	do { fprintf(stderr, "todo: %s\n", func); abort(); } while (0)

#define u_check_alloc(p)	do { if (!(p)) { perror("couldn't allocate memory"); abort(); } } while (0)

#define u_KiB(x)	((x) * (1 << 10))
#define u_MiB(x)	((x) * (1 << 20))
#define u_GiB(x)	((x) * (1 << 30))

#define u_list_head_init(h)		do { (h)->next = (h); (h)->prev = (h); } while (0)
#define u_list_head_append(h, e) \
	do {					\
		typeof(h) tail = (h)->prev;	\
		(e)->prev = tail;		\
		tail->next = (e);		\
		(e)->next = (h);		\
		(h)->prev = (e);		\
	} while (0)

struct u_arena {
	size_t pos;
	size_t cap;
	uint8_t *mem;
};

void *u_calloc(size_t n, size_t size);

struct u_arena u_arena_new(size_t cap);
void *u_arena_alloc(struct u_arena *a, size_t size);
void u_arena_free(struct u_arena *a);
void u_arena_del(struct u_arena *a);

#ifdef UTIL_H_IMPL

void *u_calloc(size_t n, size_t size)
{
	void *p = calloc(n, size);
	u_check_alloc(p);
	return p;
}

struct u_arena u_arena_new(size_t cap)
{
	return (struct u_arena) {
		.pos = 0,
		.cap = cap,
		.mem = u_calloc(cap, 1),
	};
}

void *u_arena_alloc(struct u_arena *a, size_t size)
{
	// waste of memory for small allocations, but we aren't doing small allocations so we ball
	size += size & (sizeof(void *) - 1);
	
	if (a->pos + size > a->cap) {
		fprintf(stderr, "arena at maximum capacity, couldn't allocate memory\n");
		abort();
	}

	void *p = a->mem + a->pos;
	a->pos += size;
	return p;
}

void u_arena_free(struct u_arena *a)
{
	a->pos = 0;
}

void u_arena_del(struct u_arena *a)
{
	free(a->mem);
	*a = (struct u_arena){ 0 };
}

#endif /* UTIL_H_IMPL */

#endif /* UTIL_H */

