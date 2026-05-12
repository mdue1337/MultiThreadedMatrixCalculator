# Multithreaded Matrix Calculator (MTMC)

A matrix calculator written in C that uses **pthreads** to parallelize addition, subtraction, and multiplication across large matrices. Data is stored as flat 1D arrays for cache efficiency, and a binary I/O mode is included for benchmarking with large inputs.

---

## Features

- Parallel matrix addition, subtraction, and multiplication
- User-controlled thread count
- Two I/O modes: text (`input.txt`) and binary (`m1.bin` / `m2.bin`)
- Memory-mapped binary input via `mmap` for reduced I/O overhead

---

## Build

```bash
gcc -O2 -pthread -o mtmc main.c
```

---

## Usage

```
./mtmc <threads> <operation> [-benchmark]
```

| Argument | Description |
|---|---|
| `<threads>` | Number of threads to use (positive integer) |
| `<operation>` | `+`, `-`, or `*` |
| `-benchmark` | Optional flag — enables binary I/O mode |

### Text mode
Reads from `input.txt`, writes result to `output.txt`.
```bash
./mtmc 4 '+'
./mtmc 8 '*'
```

### Benchmark mode
Reads from `m1.bin` and `m2.bin`, writes result to `output.bin`.
```bash
./mtmc 8 '*' -benchmark
```

---

## Input Format (text mode)

`input.txt` must contain exactly two matrices in the following format:

```
{
  [1, 2, 3]
  [4, 5, 6]
}
{
  [7, 8, 9]
  [1, 2, 3]
}
```

The result is written to `output.txt` in the same format.

---

## Binary Format

Binary files use a simple header followed by row-major integer data:

```
[4 bytes: rows] [4 bytes: cols] [rows*cols*4 bytes: data]
```

A helper script is included to generate binary input files for benchmarking.

---

## Benchmarks

Tested on a 5000×5000 matrix (add/subtract) and up to 3000×3000 (multiply).

| Operation | Matrix Size | 1 Thread (s) | 4 Threads (s) | 8 Threads (s) | 12 Threads (s) | 16 Threads (s) |
|---|---|---|---|---|---|---|
| Addition (+) | 5000×5000 | 0.167 | 0.145 | 0.129 | 0.144 | 0.126 |
| Subtraction (−) | 5000×5000 | 0.152 | 0.132 | 0.124 | 0.137 | 0.122 |
| Multiplication (×) | 1000×1000 | 0.575 | 0.184 | 0.135 | 0.128 | 0.130 |
| Multiplication (×) | 2000×2000 | 6.736 | 1.953 | 1.108 | 1.150 | 1.161 |
| Multiplication (×) | 3000×3000 | 50.424 | 13.340 | 7.332 | 6.616 | 6.406 |

### Speedup (relative to 1 thread)

| Operation | Matrix Size | 4 Threads | 8 Threads | 12 Threads | 16 Threads |
|---|---|---|---|---|---|
| Addition | 5000×5000 | 1.15× | 1.29× | 1.16× | 1.33× |
| Subtraction | 5000×5000 | 1.15× | 1.23× | 1.11× | 1.25× |
| Multiplication | 3000×3000 | 3.78× | 6.88× | 7.62× | 7.87× |

### Why addition scales poorly

Addition and subtraction are **memory-bound** — each element is read once and written once, so the CPU spends most of its time waiting on RAM rather than computing. Adding more threads doesn't help much because they all compete for the same memory bandwidth.

Multiplication is **compute-bound** — each output element requires a full dot product, so there is far more work per memory access. This gives threads something meaningful to parallelize, which is why you see nearly 8× speedup at 8 threads on large matrices.
