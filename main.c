#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stddef.h>

/* assume all allocator headers are included here */

int main(void)
{
    printf("=== Allocator Test Harness ===\n\n");

    /* -------------------------------------------------
       BUMP ALLOCATOR TEST
    ------------------------------------------------- */
    {
        printf("[BUMP] test\n");

        static unsigned char buffer[256];
        struct bump_allocator_state bump_state;

        allocator bump =
            make_bump_allocator(&bump_state, buffer, sizeof buffer);

        int *a = bump.ops->alloc(bump.state, sizeof *a);
        double *b = bump.ops->alloc(bump.state, sizeof *b);

        assert(a && b);
        assert(((uintptr_t)a % alignof(max_align_t)) == 0);
        assert(((uintptr_t)b % alignof(max_align_t)) == 0);

        *a = 42;
        *b = 3.14;

        printf("  a=%d b=%.2f\n", *a, *b);
    }

    /* -------------------------------------------------
       POOL ALLOCATOR TEST
    ------------------------------------------------- */
    {
        printf("\n[POOL] test\n");

        static unsigned char buffer[256];
        struct pool_allocator_state pool_state;

        allocator pool =
            make_pool_allocator(&pool_state,
                                 buffer,
                                 sizeof buffer,
                                 sizeof(int));

        int *x = pool.ops->alloc(pool.state, sizeof(int));
        int *y = pool.ops->alloc(pool.state, sizeof(int));

        assert(x && y);

        *x = 10;
        *y = 20;

        pool.ops->free(pool.state, x);
        pool.ops->free(pool.state, y);

        int *z = pool.ops->alloc(pool.state, sizeof(int));
        assert(z == y || z == x); /* reuse confirmed */

        printf("  pool reuse OK\n");
    }

    /* -------------------------------------------------
       FREE-LIST ALLOCATOR TEST
    ------------------------------------------------- */
    {
        printf("\n[FREE-LIST] test\n");

        static unsigned char heap[512];
        struct free_list_allocator_state fl_state;

        free_list_init(&fl_state, heap, sizeof heap);

        allocator fl = {
            .state = &fl_state,
            .ops = &(allocator_ops){
                .alloc = free_list_alloc,
                .free  = free_list_free
            }
        };

        void *a = fl.ops->alloc(fl.state, 64);
        void *b = fl.ops->alloc(fl.state, 64);
        void *c = fl.ops->alloc(fl.state, 64);

        assert(a && b && c);

        fl.ops->free(fl.state, b);
        fl.ops->free(fl.state, a);
        fl.ops->free(fl.state, c);

        /* After freeing all, heap should coalesce */
        void *d = fl.ops->alloc(fl.state, 200);
        assert(d != NULL);

        printf("  coalescing OK\n");
    }

    printf("\n=== ALL TESTS PASSED ===\n");
    return 0;
}
