# Real-Time Executive (RTX) Implementation on ARM Cortex-M4 (ECE-350)

This repo contains the work done for the lab component of ECE 350 at the University of Waterloo. The code was written by myself (Nicholas Cantone), Sharanya Gupta, Andrey Golovanov and Joshua Leveck. 

---

This project implements a Real-Time Executive (RTX) on an ARM Cortex-M4 microcontroller, focusing on a custom memory management system, task scheduling, and real-time task management. The implementation follows the guidelines of embedded system design and real-time operating systems (RTOS) principles.

## Overview

The RTX system is designed to manage multiple concurrent tasks, each with its own execution context, and ensure real-time performance. Key features include:

- **Memory Management:** A buddy system-based allocator.
- **Task Scheduling:** Priority-based scheduling with a focus on deadlines.
- **System Calls Handling:** Custom SVC (Supervisor Call) handling for task management.

## Memory Management

The memory management subsystem uses a buddy system to manage dynamic memory allocation efficiently. It divides the available memory into blocks of various sizes (powers of 2) and keeps track of free and allocated blocks using a bit array and free lists.

### Key Components
**1. Buddy System Allocator**:
 - The memory is divided into blocks, each corresponding to a tree node in the buddy system.
 - A bit array tracks the state of each node (free/allocated), and a set of free lists manages free blocks for each size.

**2. Helper Functions:**
- **index_to_level:** Maps a bit array index to its tree level.
- **get_buddy:** Identifies the buddy of a given block to facilitate merging during deallocation.
- **split_node:** Splits a larger block into two smaller blocks when necessary (in allocation).
- **addr_to_index:** Converts a memory address to the corresponding bit array index, determining the block’s position.

**3. Allocation and Deallocation:**
- **k_mem_alloc:** Allocates memory by finding the smallest available block that fits the requested size. If no such block exists, it splits a larger block.
- **k_mem_dealloc:** Frees a block of memory and coalesces it with its buddy, if free, to form a larger block.

### Design Considerations
- **Efficiency:** The buddy system ensures minimal internal fragmentation and supports fast coalescence of adjacent free blocks.
- **Robustness:** Memory block validity is verified using metadata to prevent erroneous deallocations.

## Task Management

The task management system maintains multiple tasks, each represented by a Task Control Block (TCB), and supports task creation, deletion, and context switching.

### Key Components
**1. Task Control Block (TCB):**
- Contains task information, including stack pointers, task state, deadline, and priority.

**2. Task Scheduling:**
- Tasks are scheduled based on an earliest-deadline-first algorithm. The scheduler selects the task with the earliest deadline that is ready to run.
- A null task ensures the CPU never enters an idle state by executing when no other tasks are available.

**3. Context Switching:**
- The context switch mechanism saves the current task's state and loads the next task's state, ensuring seamless multitasking.
### Design Considerations
- **Deadline-Driven Scheduling:** Ensures high-priority tasks meet their deadlines, critical for real-time systems.
- **Null Task:** Prevents the system from entering an idle state, maintaining continuous operation.

## System Calls Handling

System calls are handled via the SVC (Supervisor Call) mechanism. Each system call is identified by an immediate value embedded in the SVC instruction, allowing the OS to perform privileged operations such as task creation, deletion, or memory allocation.

#### Key Components
**1. SVC_Handler_Main:**
- Determines the system call type based on the SVC number and executes the corresponding operation.
- Handles context switching and memory management operations in response to system calls.
#### Design Considerations
- **Modularity:** The use of SVCs allows the system to extend functionality by adding new system calls without modifying the core OS.
- **Security:** The use of SVCs enforces privilege separation, protecting the kernel from unprivileged code.

## Conclusion

This RTX implementation on the ARM Cortex-M4 is designed to provide efficient, real-time task management and dynamic memory allocation with minimal overhead. The combination of a buddy system for memory management, a priority-deadline scheduler, and robust system call handling makes it a powerful foundation for real-time applications in embedded systems.

