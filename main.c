#include <stddef.h>
#include "alloc.h"

static unsigned char buffer[128];
struct bump_allocator_state state;
allocator alloc = make_bump_allocator(
    &state,
    buffer,
    sizeof buffer
);
// Allocate int
int *i = alloc.ops->alloc(alloc.state, sizeof *i);
// Allocate double
double *d = alloc.ops->alloc(alloc.state, sizeof *d);
// Allocate struct
struct pair {
    int a;
    double b;
};
struct pair *p = alloc.ops->alloc(alloc.state, sizeof *p);
