#include <stdlib.h>

struct allocator_ops {
        void *(*alloc)(void* state, size_t size);
        void (*free)(void* state, void * ptr);
    };
    
    struct allocator {
        void * state;
        struct allocator_ops * ops;
    };
    
    struct bump_allocator_state {
        size_t size;
        unsigned char * base;
        size_t offset;
    };
    
    void *bump_alloc(void *state, size_t size) {
        struct bump_allocator_state* s = (struct bump_allocator_state*)state;
        size_t alignment = alignof(max_align_t);
        s->offset = (s->offset + alignment - 1) & ~(alignment-1);
        if(s->offset + size > s->size) {
            return NULL;
        }
        unsigned char * ptr = s->base + s->offset;
        s->offset += size;
        return (void*)ptr;
    }
    
    void bump_free(void *state, void *ptr) {
        struct bump_allocator_state* s = (struct bump_allocator_state*)state;
        (void)base;
        (void)ptr;
    }
    
    static const allocator_ops bump_allocator_ops = { bump_alloc, bump_free };
    
    allocator make_bump_allocator(
        struct bump_allocator_state *state,
        void *memory,
        size_t size
    ) {
        state->base = (unsigned char*)memory;
        state->size = size;
        state->offset = 0;
        
        return (allocator){state, &bump_allocator_ops};
    }
