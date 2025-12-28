#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdalign.h>

struct allocator_ops
{
    void *(*alloc)(void *state, size_t size);
    void (*free)(void *state, void *ptr);
};

struct allocator
{
    void *state;
    struct allocator_ops *ops;
};

struct bump_allocator_state
{
    size_t size;
    unsigned char *base;
    size_t offset;
};

void *bump_alloc(void *state, size_t size)
{
    struct bump_allocator_state *s = (struct bump_allocator_state *)state;
    size_t alignment = alignof(max_align_t);
    s->offset = (s->offset + alignment - 1) & ~(alignment - 1);
    if (s->offset + size > s->size)
    {
        return NULL;
    }
    unsigned char *ptr = s->base + s->offset;
    s->offset += size;
    return (void *)ptr;
}

void bump_free(void *state, void *ptr)
{
    struct bump_allocator_state *s = (struct bump_allocator_state *)state;
    (void)state->base;
    (void)ptr;
}

static const allocator_ops bump_allocator_ops = {bump_alloc, bump_free};

allocator make_bump_allocator(
    struct bump_allocator_state *state,
    void *memory,
    size_t size)
{
    state->base = (unsigned char *)memory;
    state->size = size;
    state->offset = 0;

    return (allocator){state, &bump_allocator_ops};
}

struct pool_allocator_state
{
    unsigned char *base;
    void *free_list;
    size_t block_size;
};

void pool_init(
    struct pool_allocator_state *state,
    void *memory,
    size_t total_size,
    size_t block_size)
{
    // Enforce minimum block size
    if (block_size < sizeof(void *))
    {
        state->free_list = NULL;
        return;
    }

    // Align base to pointer alignment
    size_t alignment = alignof(void *);
    unsigned char *base = (unsigned char *)((uintptr_t)memory + alignment - 1) & ~(alignment - 1);
    unsigned char *end = (unsigned char *)memory + total_size;

    state->base = base;
    state->block_size = block_size;
    state->free_list = base;

    // Calculate how many whole blocks fit
    size_t blocks = ((size_t)(end - base)) / block_size;

    if (blocks == 0)
    {
        return;
    }

    // Build free list in-place
    unsigned char *current = state->free_list;
    for (size_t i = 0; i < blocks - 1; ++i)
    {
        *(void **)current = current + block_size;
        current = *(void **)current;
    }
    *(void **)current = NULL;
}
