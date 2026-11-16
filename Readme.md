# High-Performance C Hash Table (`ht`)

A cache-friendly, open-addressing string-to-string hash table written in strict C17. Designed with **mechanical sympathy** to maximize CPU L1/L2 cache locality, leverage hardware prefetchers, and prevent memory leaks through rigorous sanitizer verification.

---

## Architecture & Mechanical Sympathy

### 1. Collision Resolution: Open Addressing vs. Separate Chaining
In languages like Java (`java.util.HashMap`), hash tables typically rely on separate chaining via linked lists. In C, linked lists cause severe cache thrashing: traversing nodes requires chasing heap pointers across non-contiguous memory, resulting in frequent CPU stalls (~100–300 cycles per main memory fetch).

This implementation uses **Open Addressing with Linear Probing**:
* **Contiguous Memory:** Entries are stored in a single, contiguous array.
* **Cache Line Exploitation:** On modern x86/ARM CPUs, memory is fetched in 64-byte **Cache Lines**. At 24 bytes per entry, a single cache line brings ~2.6 entries into L1 SRAM. During linear probing, consecutive collisions hit L1/L2 cache (~1–4 cycles) rather than DRAM.
* **Prefetcher Friendly:** The CPU hardware prefetcher detects the sequential memory access pattern and streams upcoming entries into cache before instructions request them.

### 2. Memory Layout & Struct Alignment
Each bucket is defined as:
```c
typedef struct {
    char *key;            // 8 bytes (64-bit pointer)
    char *value;          // 8 bytes (64-bit pointer)
    bucket_state_t state; // 4 bytes (enum / int)
                          // 4 bytes implicit compiler padding
} entry_t;                // Total: 24 bytes (8-byte aligned)
```
* The struct is 24 bytes, aligned to an 8-byte boundary.
* Zero-initialized at allocation via `calloc()`, setting all states to `BUCKET_EMPTY (0)` and pointers to `NULL` in a single OS-optimized operation.

### 3. Deletion Strategy: Tombstones
To prevent breaking probe chains during deletion without shifting elements:
* Deleting a key sets the bucket state to `BUCKET_TOMBSTONE` and frees the associated strings.
* Lookups probe past tombstones, while insertions can reclaim them once the probe chain confirms no duplicate key exists downstream.
* All tombstones are completely purged during rehashing.

### 4. Load Factor & Resizing Mechanics (Why 70%?)
Linear probing suffers from **Primary Clustering** (the tendency of filled slots to form contiguous blocks, increasing collision cascades). According to Donald Knuth's probe formula for linear probing:

$$\text{Average Probes} \approx \frac{1}{2} \left(1 + \frac{1}{(1 - \alpha)^2}\right)$$

* At **$\alpha = 0.50$**: ~2.5 average probes.
* At **$\alpha = 0.70$**: ~6.0 average probes (~1–3 cache lines).
* At **$\alpha \ge 0.80$**: Average probes jump to $13.0+$, degrading $O(1)$ amortized lookups into an $O(N)$ linear scan.

A maximum load factor of **0.70 (70%)** was selected as the optimal trade-off between memory utilization and cache latency. When $\alpha \ge 0.70$, the table doubles in capacity and rehashes all active entries.

### 5. Hashing Algorithm: 64-bit FNV-1a
Uses the non-cryptographic **FNV-1a** hash algorithm:
* Scrambles input bytes via bitwise XOR and multiplication by the FNV prime (`0x100000001b3ULL`).
* Excellent avalanche characteristics with minimal CPU cycles per byte.
* Characters are explicitly cast to `uint8_t` before hashing to eliminate platform-dependent sign-extension bugs.

---

## Project Structure

```text
.
├── Makefile
├── include/
│   └── hashtable.h          # Public API (Opaque struct encapsulation)
├── src/
│   └── hashtable.c          # Internal implementation & data structures
├── tests/
│   └── test_hashtable.c     # Unit test suite & edge-case validation
└── bench/
    └── bench_hashtable.c    # High-resolution POSIX latency/throughput benchmark
```

---

## Public API (`include/hashtable.h`)

Encapsulation is enforced using an **opaque pointer** (`typedef struct ht ht_t;`). Callers cannot inspect or manipulate internal memory fields directly.

```c
ht_t*       ht_create(void);
void        ht_destroy(ht_t *table);
bool        ht_put(ht_t *table, const char *key, const char *value);
const char* ht_get(ht_t *table, const char *key);
bool        ht_delete(ht_t *table, const char *key);
size_t      ht_count(ht_t *table);
```

* **Memory Ownership:** `ht_put()` duplicates both key and value strings via `strdup()`. The hash table owns its internal strings and frees them automatically during `ht_delete()` or `ht_destroy()`.

---

## Build & Verification

### Prerequisites
* GCC or Clang
* POSIX-compliant environment (Linux / macOS)

### 1. Run Unit Tests (with ASan & UBSan)
Tests are compiled with `-O0 -g -fsanitize=address,undefined` to instantly halt on memory leaks, out-of-bounds accesses, use-after-free, or undefined behavior:

```bash
make test
```

### 2. Run High-Resolution Benchmarks
Benchmarks are compiled with `-O3` optimizations:

```bash
make bench
```

---

## Benchmark Results

*Hardware: x86_64 Linux, GCC 11+ (`-O3`)*  
*Payload: 200,000 operations, geometric resizing from 16 to 524,288 buckets.*

| Operation | Total Time | Throughput | Latency |
| :--- | :--- | :--- | :--- |
| **`PUT` (Insert + 15 Resizes)** | `0.0478 s` | **~4.18 Million ops/sec** | **~239.21 ns/op** |
| **`GET` (Cache Hit)** | `0.0125 s` | **~16.00 Million ops/sec** | **~62.50 ns/op** |
| **`GET` (Cache Miss)** | `0.0098 s` | **~20.40 Million ops/sec** | **~49.02 ns/op** |

---
