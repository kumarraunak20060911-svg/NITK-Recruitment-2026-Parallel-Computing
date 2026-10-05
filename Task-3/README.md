# Task 3: Forest Fire Simulation via ESSL 3.10 Compute Shaders

## Overview

This implementation simulates a forest fire spreading on a two-dimensional `M × M` grid using an OpenGL ES 3.1 compute shader.

The host program is written in C and uses EGL to create an OpenGL ES context. The cell-state update is performed in parallel on the GPU using ESSL 3.10 compute shaders.

The program is designed for the Android/Termux environment required by the task and can also be compiled and tested on Linux/WSL with Mesa.

## State Representation

Each grid cell has one of three states:

| State | Value | Output |
|---|---:|---|
| Healthy | `0` | `H` |
| Burning | `1` | `B` |
| Nothing | `2` | `.` |

The initial grid is completely Healthy except for the centre cell, which is set to Burning.

## Simulation Rules

The simulation advances in discrete epochs.

For every epoch:

1. A Burning cell becomes Nothing.
2. A Healthy cell becomes Burning with probability `p = 0.15` if at least one of its eight neighbouring cells is Burning.
3. Otherwise, the cell keeps its current state.
4. Nothing cells remain Nothing.
5. The simulation terminates when no Burning cells remain.

The neighbourhood is the eight-connected Moore neighbourhood:

```text
NW  N  NE
 W  C   E
SW  S  SE
