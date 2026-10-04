# High-Performance Multi-Threaded 64-Bit Array Reduction

A low-level C implementation comparing parallel workload decomposition strategies for 64-bit unsigned integer reductions using POSIX Threads (pthreads).

---

## Technical Highlights

- Hardware Memory Alignment: Dynamic memory allocation via posix_memalign aligned to 64-byte boundaries (L1 cache line width) to prevent unaligned memory access penalties.
- Cache-Line Padding (alignas(64)): Thread-local accumulation structures isolated on distinct cache lines to eliminate L1 Cache Line Bouncing (False Sharing).
- Instruction-Level Parallelism (ILP): Manual 4-way register unrolling breaking data dependency chains to maximize CPU execution pipeline throughput.
- Zero Heavy Dependencies: Pure C11 code relying strictly on Standard C libraries and libpthread.

---

## Parallel Strategies

Given an array A of size N >= 1024 and thread index i in {0, 1, 2, 3}:

### 1. Strategy I: Cyclic / Interleaved Decomposition
Each thread i computes partial sums of elements at indices x matching:
x % 4 == i

- Memory Pattern: Non-contiguous strided access (32 bytes stride).
- Cache Behavior: Multiple CPU cores compete for identical or adjacent 64-byte cache lines, causing cache thrashing and pipeline stalls.

### 2. Strategy II: Block / Contiguous Decomposition
Each thread i computes partial sums across a contiguous range:
Range_i = [i * (N / 4), (i + 1) * (N / 4))

- Memory Pattern: Pure linear sequential streaming.
- Cache Behavior: Maximizes spatial locality. Loading a single 64-byte cache line brings 8 uint64_t elements directly into L1/L2 cache for execution by a single core.

---

## Build and Execution

### Compilation
Compile using GCC with Level 2 optimizations (-O2) and POSIX Threads linking:

```bash
gcc -O2 src/task1.c -lpthread -o task1
