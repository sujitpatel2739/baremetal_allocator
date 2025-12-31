# Common allocator interface

This repository contains a complete, from-scratch implementation of three fundamental
memory allocator strategies in **ISO C (C11)**, designed for systems programming,
OS-level development, and high-performance backends.

The goal is not to wrap `malloc`, but to **understand and implement allocator internals**
including alignment, metadata, splitting, coalescing, and polymorphic interfaces.

---

## Allocators Implemented

### 1. Bump Allocator
**Strategy:** Linear allocation, no reuse  
**Free:** No-op  
**Use cases:**
- Arena allocation
- Request-scoped memory
- Temporary objects
- Extremely fast allocation

**Properties:**
- O(1) allocation
- No fragmentation
- Requires reset to reuse memory

---

### 2. Pool Allocator
**Strategy:** Fixed-size blocks with reuse  
**Free:** O(1)  
**Use cases:**
- Object pools
- Networking buffers
- Real-time systems

**Properties:**
- O(1) allocation and free
- Zero fragmentation
- Block size fixed at initialization

---

### 3. Free-List Allocator (malloc-class)
**Strategy:** Variable-size blocks with splitting & coalescing  
**Free:** O(n) worst-case (first-fit free list)  
**Use cases:**
- General-purpose heap
- Embedded malloc replacement
- Learning real allocator internals

**Features:**
- Boundary tags (header + footer)
- First-fit allocation
- Block splitting
- Backward + forward coalescing
- Alignment to `max_align_t`

---

## Architecture

All allocators share a **common polymorphic interface**:

```c
struct allocator_ops {
    void *(*alloc)(void *state, size_t size);
    void  (*free)(void *state, void *ptr);
};

struct allocator {
    void *state;
    const struct allocator_ops *ops;
};
````

This allows allocator types to be swapped **without changing client code**.

---

## Design Principles

* No global state
* No hidden allocations
* Strict-aliasing safe
* Alignment-correct
* Explicit ownership
* Header-only metadata where possible
* Minimal, correct invariants

---

## Build

```bash
gcc -std=c11 -Wall -Wextra -O2 main.c -o alloc_test
```

---

## Status

✔ Bump allocator
✔ Pool allocator
✔ Free-list allocator
✔ Alignment handling
✔ Splitting & coalescing
✔ Polymorphic interface
