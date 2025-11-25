# Dining Philosophers Problem

Implementation of the **Dining Philosophers Problem** in C using processes, threads, and shared memory.

## Overview
The Dining Philosophers problem is a classic concurrency problem that illustrates synchronization issues and deadlock prevention. This project:

- Uses **forks** and **mutexes** to manage shared resources.
- Implements **processes and threads** for each philosopher.
- Demonstrates **deadlock-free scheduling** and resource sharing.
- Shows philosophers thinking, eating, and releasing forks.

## Features
- Thread-safe resource allocation
- Dynamic number of philosophers
- Round-robin scheduling with time quantum
- Console-based simulation with descriptive outputs

## How to Run
1. Compile the code using GCC with pthread support:
   ```bash
   gcc src/dining_philosophers.c -o dining_philosophers -lpthread
