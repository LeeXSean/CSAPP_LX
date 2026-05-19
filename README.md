<div align="center">

# CS:APP Labs

*Computer Systems: A Programmer's Perspective* (3rd ed.) Lab Solutions

</div>

---

Solutions to the lab assignments from CMU's CS:APP course. Click any lab below for details.

<details>
<summary>✅ <a href="Data_Lab/"><b>Data Lab</b></a> — Bit manipulation, integer/float encoding</summary>

> Implement integer and floating-point operations using only bitwise operators,
> under strict constraints on allowed operators and operation count.

</details>

<details>
<summary>✅ <a href="Bomb_Lab/"><b>Bomb Lab</b></a> — Reverse engineering, GDB, x86-64 assembly</summary>

> Defuse a binary bomb by reverse engineering its phases with GDB.
> Includes cracking a hidden phase backed by a binary tree recursive search.

</details>

<details>
<summary>✅ <a href="Attack_Lab/"><b>Attack Lab</b></a> — Buffer overflow, ROP chain, ASLR bypass</summary>

> Exploit stack buffer overflows to hijack control flow.
> - Phase 1–3: code injection
> - Phase 4–5: ROP chain to bypass NX and ASLR

</details>

<details>
<summary>✅ <a href="Cache_Lab/"><b>Cache Lab</b></a> — Cache simulation, matrix transpose optimization</summary>

> **Part A** — Implement an LRU cache simulator in C,
> supporting arbitrary `(S, E, b)` parameters.
>
> **Part B** — Optimize matrix transpose for cache performance.
> The 64x64 case splits each 8x8 block into four 4x4 sub-blocks,
> repurposing unused regions of B as a staging buffer to minimize conflict misses.

</details>

<details>
<summary>✅ <a href="Shell_Lab/"><b>Shell Lab</b></a> — Processes, signals, job control</summary>

> Implement a Unix shell (`tsh`) with:
> - Foreground / background job control
> - Signal handling (`SIGINT`, `SIGTSTP`, `SIGCHLD`)
> - Built-in commands: `jobs`, `fg`, `bg`, `quit`
>
> Signal masking eliminates race conditions between `fork` and `addjob`.

</details>

<details>
<summary>✅ <a href="Malloc_Lab/"><b>Malloc Lab</b></a> — Dynamic memory allocator, segregated free lists</summary>

> Implement `malloc`, `free`, and `realloc` from scratch.
>
> - Segregated explicit free lists with 12 size classes
> - Boundary tags with `prev_alloc` bit to eliminate footers on allocated blocks
> - Circular doubly-linked free lists; each class stores one head pointer
> - Immediate coalescing on every free
> - `realloc` tries in-place growth (next block, heap end, prev block, both
>   neighbors) before falling back to malloc-copy-free
>
> Score: **97 / 100**

</details>

- 🔄 Proxy Lab — In progress

---

## Acknowledgment

Special thanks to [virgiling](https://github.com/virgiling) for the helpful [blog](https://virgiling.wiki/) that provided great guidance throughout these labs.
