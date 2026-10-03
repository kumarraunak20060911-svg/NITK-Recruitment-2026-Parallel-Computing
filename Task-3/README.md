# Task 3: Forest Fire Cellular Automaton (ESSL & EGL)

A parallel simulation of a Forest Fire Cellular Automaton executed on GPU compute hardware via EGL and ESSL 3.10 Compute Shaders.

## Problem Statement

The simulation tracks tree status across a 2D grid (M x M) evolving over discrete time steps (epochs) based on three transition rules:
1. **Burning (1) -> Burnt/Nothing (2):** A burning tree burns out completely in the next epoch.
2. **Healthy (0) -> Burning (1):** A healthy tree catches fire in the next epoch with probability p = 0.15 if at least one adjacent neighbor (8-neighborhood) is currently burning.
3. **Burnt/Nothing (2) -> Burnt/Nothing (2):** Burnt state remains unchanged.

The simulation terminates automatically when zero burning trees remain.

## Implementation Details

- **Headless GPU Acceleration:** Uses **EGL** to establish an off-screen OpenGL ES 3.1 context without requiring a display server or window manager.
- **Compute Shader:** Written in **ESSL 3.10** and embedded directly in the C source code for lightweight deployment.
- **Data Transfer:** Employs **Shader Storage Buffer Objects (SSBOs)** with double-buffering for lock-free GPU state updates.
- **Host-Device Sync:** Transfers frame state back to host memory using `glMapBufferRange`.

## Project Structure

```text
Task-3/
├── README.md
└── src/
    └── task3.c
