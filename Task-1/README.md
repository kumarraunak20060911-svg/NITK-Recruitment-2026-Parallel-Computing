# Task 1: Parallel Array Summation using Pthreads

## Overview
This program calculates the sum of a large array of 64-bit unsigned integers (N >= 1024) using C and the POSIX pthreads API. 

It compares two multi-threaded execution strategies using 4 threads against a single-threaded sequential baseline:
1. **Sequential Baseline:** A single thread iterates sequentially from index 0 to N-1.
2. **Strategy i (Cyclic Decomposition):** Thread i computes elements at index x where `x % 4 == i`.
3. **Strategy ii (Block Decomposition):** Thread i computes elements in the range `i * (N / 4)` to `(i + 1) * (N / 4)`.

---

## File Structure
- `src/task1.c` - Complete C source code containing sequential, cyclic, and block summation strategies along with benchmarking timing logic.

---

## How to Build and Run

### Prerequisites
- GCC Compiler
- Linux environment (or WSL / Termux)

### Commands
Compile the code with Optimization (-O2) and POSIX Threads (-pthread):
```bash
gcc -O2 -pthread src/task1.c -o task1
