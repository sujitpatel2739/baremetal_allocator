#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
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

// Pool allocator --------------------------------------------------------------

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
    unsigned char *base = (unsigned char *)(((uintptr_t)memory + alignment - 1) & ~(alignment - 1));
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

void *pool_alloc(void *state)
{
    struct pool_allocator_state *s = (struct pool_allocator_state *)state;
    if (s->free_list == NULL)
    {
        return NULL;
    }
    void *block = s->free_list;
    s->free_list = *(void **)s->free_list;
    return block;
}

void pool_free(void *state, void *ptr)
{
    struct pool_allocator_state *s = (struct pool_allocator_state *)state;
    *(void **)ptr = s->free_list;
    s->free_list = ptr;
}

static const allocator_ops pool_allocator_ops = {pool_alloc, pool_free};

allocator make_pool_allocator(
    struct pool_allocator_state *state,
    void *memory,
    size_t total_size,
    size_t block_size)
{
    pool_init(
        state,
        memory,
        total_size,
        block_size);

    return (allocator){state, &pool_allocator_ops};
}

// Free-List allocator -----------------------------------------------------

// header: size, LSB bit allocation flag, a free list pointer
// footer: size, a free list pointer

// Macros/Inline helpers for:
// - Extracting block size (masking flags)
// - Checking if a block is allocated
// - Marking a block allocated / free

typedef size_t block_meta;
#define ALLOCATED ((size_t)1)

#define BLOCK_SIZE(m) ((m) & ~ALLOCATED)
#define IS_ALLOCATED(m) ((m) & ALLOCATED)
#define MARK_ALLOCATED(m) ((m) | ALLOCATED)
#define MARK_FREE(m) ((m) & ~ALLOCATED)
#define MIN_SIZE_REQ (sizeof(block_meta) + sizeof(void *) + sizeof(block_meta))

// static inline helpers

static inline void *payload_from_header(block_meta *h)
{
    return (void *)((char *)h + sizeof(block_meta));
}
static inline block_meta *header_from_payload(void *p)
{
    return ((block_meta *)(p)-1);
}
static inline block_meta *footer_from_header(block_meta *h)
{
    return (block_meta *)((char *)h + BLOCK_SIZE(*h) - sizeof(block_meta));
}
static inline block_meta *header_from_footer(block_meta *f)
{
    return (block_meta *)((char *)f - BLOCK_SIZE(*f) + sizeof(block_meta));
}

struct free_list_allocator_state
{
    unsigned char *heap_start;
    block_meta *free_list;
    unsigned char *heap_end;
};

void free_list_init(
    struct free_list_allocator_state *state,
    void *memory,
    size_t size)
{
    size_t alignment = alignof(max_align_t);
    uintptr_t aligned = ((uintptr_t)memory + alignment - 1) & ~(alignment - 1);
    unsigned char *heap_start = (unsigned char *)aligned;
    unsigned char *heap_end = (unsigned char *)memory + size;

    // calculating and rounding of the block_size
    size_t block_size = (size_t)(heap_end - heap_start);
    block_size &= ~(alignment - 1);

    // Minimum block size check
    if (MIN_SIZE_REQ > block_size)
    {
        state->free_list = NULL;
        return;
    }

    state->heap_start = heap_start;
    state->heap_end = heap_start + block_size;
    state->free_list = (block_meta *)base;
    *state->free_list = MARK_FREE(block_size);
    *(void **)payload_from_header(state->free_list) = NULL;
    *footer_from_header(state->free_list) = MARK_FREE(block_size);
}

void *free_list_alloc(void *state, size_t size)
{
    struct free_list_allocator_state *fls = (struct free_list_allocator_state *)state;

    size_t alignment = alignof(max_align_t);
    size = max(size, sizeof(void *));
    size = (size + alignment - 1) & ~(alignment - 1);
    size_t required_size = size + sizeof(block_meta) * 2;

    block_meta *curr_block = fls->free_list;
    if (curr_block == NULL)
        return NULL;

    block_meta *prev_block = NULL;
    size_t available = (size_t)BLOCK_SIZE(*curr_block);
    while (curr_block != NULL && available < required_size)
    {
        prev_block = curr_block;
        curr_block = (block_meta *)*(block_meta **)payload_from_header(curr_block);
        if (curr_block == NULL)
            return NULL;
        available = (size_t)BLOCK_SIZE(*curr_block);
    }
    if (curr_block == NULL || available < required_size)
    {
        return NULL;
    }

    size_t remainder = available - required_size;
    block_meta *next_free = (block_meta *)*(block_meta **)payload_from_header(curr_block);
    if (remainder >= MIN_SIZE_REQ)
    {

        *curr_block = MARK_ALLOCATED(required_size);
        *footer_from_header(curr_block) = MARK_ALLOCATED(required_size);

        block_meta *remain_header = (block_meta *)((char *)curr_block + required_size);
        *remain_header = MARK_FREE(remainder);
        *footer_from_header(remain_header) = MARK_FREE(remainder);

        *(block_meta **)payload_from_header(remain_header) = next_free;
        next_free = remain_header;
    }
    else
    {
        *curr_block = MARK_ALLOCATED(available);
        *footer_from_header(curr_block) = MARK_ALLOCATED(available);
    }
    if (prev_block == NULL)
    {
        fls->free_list = next_free;
    }
    else
    {
        *(block_meta **)payload_from_header(prev_block) = next_free;
    }

    return payload_from_header(curr_block);
}

void free_list_free(void *state, void *ptr)
{
    struct free_list_allocator_state *fls = (struct free_list_allocator_state *)state;

    block_meta *header = header_from_payload(ptr);
    size_t size = BLOCK_SIZE(*header);

    *header = MARK_FREE(size);
    *footer_from_header(header) = MARK_FREE(size);
    block_meta *coalesced_header = header;

    if ((char *)header > fls->heap_start)
    {
        block_meta *prev_footer = (block_meta *)((char *)header - sizeof(block_meta));

        if (!IS_ALLOCATED(*prev_footer))
        {
            block_meta *prev_header = header_from_footer(prev_footer);
            if ((char *)prev_header + BLOCK_SIZE(*prev_header) == (char *)header)
            {
                block_meta **p = &fls->free_list;
                while (*p && *p != prev_header)
                    p = (block_meta **)payload_from_header(*p);
                if (*p == prev_header)
                    *p = *(block_meta **)payload_from_header(prev_header);
                size += BLOCK_SIZE(*prev_header);
                coalesced_header = prev_header;
            }
        }
    }

    if ((char *)coalesced_header + size + sizeof(block_meta) <= fls->heap_end)
    {
        block_meta *next_header = (block_meta *)((char *)coalesced_header + size);
        if (!IS_ALLOCATED(*next_header) && (char *)coalesced_header + size == (char *)next_header)
        {
            block_meta **p = &fls->free_list;
            while (*p && *p != next_header)
                p = (block_meta **)payload_from_header(*p);
            if (*p == next_header)
                *p = *(block_meta **)payload_from_header(next_header);
            size += BLOCK_SIZE(*next_header);
        }
    }

    *coalesced_header = MARK_FREE(size);
    *footer_from_header(coalesced_header) = MARK_FREE(size);
    *(block_meta **)payload_from_header(coalesced_header) = fls->free_list;
    fls->free_list = coalesced_header;
}
