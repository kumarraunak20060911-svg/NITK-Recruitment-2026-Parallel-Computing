# NITK Parallel Computing Recruitment 2026

This repository contains high-performance, clean C implementations for three parallel computing recruitment tasks. The implementations focus on efficiency, minimal memory overhead, thread safety, and cross-platform hardware acceleration.

---

## Task Overview

| Task | Title | Description | Technologies / Strategy |
| :--- | :--- | :--- | :--- |
| **Task 1** | Array Summation | Parallel reduction over dynamic integer arrays | POSIX Threads (`pthreads`), Cyclic & Block Distribution |
| **Task 2** | SPSC Ring Buffer | Lock-free Single-Producer Single-Consumer FIFO queue | C11 Atomics (`stdatomic.h`), Bitwise Indexing (Power of 2) |
| **Task 3** | Forest Fire Simulation | 2D Cellular Automaton with stochastic propagation | EGL Headless Context, ESSL 3.10 Compute Shader, SSBOs |

---

## Repository Structure

```text
.
├── Task-1/
│   ├── README.md
│   └── src/
│       └── task1.c
├── Task-2/
│   ├── README.md
│   └── src/
│       └── task2.c
├── Task-3/
│   ├── README.md
│   └── src/
│       └── task3.c
├── .gitignore
└── README.md
