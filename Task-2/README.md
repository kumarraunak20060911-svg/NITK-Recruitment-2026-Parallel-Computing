# High-Performance Lock-Free SPSC Ring Buffer

A low-level C11 implementation of a Single-Producer Single-Consumer (SPSC) lock-free ring buffer designed for low latency and high throughput using atomic memory ordering semantics and cache-line alignment.

---

## Technical Highlights

- **Cache-Line Isolation (`alignas(64)`)**: Prevents false sharing (cache line bouncing) between reader and writer cores by forcing `head` and `tail` atomic pointers into separate 64-byte cache lines.
- **Acquire-Release Memory Ordering**: Eliminates lock contention by using explicit `memory_order_relaxed`, `memory_order_acquire`, and `memory_order_release` semantics instead of sequentially consistent atomic operations or heavy OS mutexes.
- **Power-of-Two Modulo Masking**: Indexing uses fast bitwise AND (`idx & RING_MASK`) rather than expensive CPU integer division/modulo instructions (`idx % RING_CAPACITY`).
- **Platform CPU Yielding**: Employs low-latency CPU pause instructions (`_mm_pause()` for x86, `isb` for ARM) inside spin loops to reduce CPU bus contention and pipeline stalls.

---

## Memory Consistency & Synchronization Model

The lock-free synchronization maintains a strict **happens-before** relationship without mutex locks:

### Producer Routine (`spsc_push`)
1. Reads `head` with `memory_order_relaxed` (thread-local state).
2. Reads `tail` with `memory_order_acquire` to ensure it sees up-to-date consumer tail position.
3. Writes the item to `ring[head & RING_MASK]`.
4. Stores the updated `head` with `memory_order_release`, committing memory writes to the consumer thread.

### Consumer Routine (`spsc_pop`)
1. Reads `tail` with `memory_order_relaxed` (thread-local state).
2. Reads `head` with `memory_order_acquire` to guarantee all data written prior to head's release store is visible.
3. Reads the item from `ring[tail & RING_MASK]`.
4. Stores the updated `tail` with `memory_order_release` to signal available capacity back to the producer thread.

---

## Building and Running

### Compilation
Compile with `-O2` optimizations and standard POSIX thread support:

```bash
gcc -O2 src/task2.c -lpthread -o task2
