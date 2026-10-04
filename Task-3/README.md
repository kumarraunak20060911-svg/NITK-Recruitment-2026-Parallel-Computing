# Task 3: Forest Fire Simulation via ESSL 3.10 Compute Shaders

## Overview
This repository contains an implementation of a two-dimensional cellular automaton simulating a forest fire spreading across an $M \times M$ grid. The compute workload is offloaded to the GPU using ESSL 3.10 compute shaders managed via EGL 1.4+ and OpenGL ES 3.1+ APIs on Android Termux.

## State Transitions & Rules
Each cell in the grid represents a tree in one of three states:
- `Healthy (H)`: Value `0`
- `Burning (B)`: Value `1`
- `Nothing (N)`: Value `2`

Simulation dynamics per epoch:
1. `Burning (B)` cells transition to `Nothing (N)`.
2. `Healthy (H)` cells transition to `Burning (B)` with probability $p = 0.15$ if at least one 8-way neighbor is currently `Burning (B)`.
3. All other cells retain their state.
4. Execution halts when zero `Burning (B)` cells remain.

## System Requirements
- Android NDK / Termux environment
- libEGL (`-lEGL`)
- libGLESv3 (`-lGLESv3`)
- GCC / Clang C compiler

## Project Structure
