#include "hashtable.h"
#include <stdio.h>
#include <time.h>

#define NUM_OPERATIONS 200000

static double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

int main(void) {
    printf("=== HASH TABLE BENCHMARK (%d items) ===\n", NUM_OPERATIONS);

    ht_t *ht = ht_create();
    char key_buf[32];
    char val_buf[32];

    // 1. Benchmark Insertions (with automatic resizing)
    double start = get_time_sec();
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        snprintf(key_buf, sizeof(key_buf), "key_%d", i);
        snprintf(val_buf, sizeof(val_buf), "val_%d", i);
        ht_put(ht, key_buf, val_buf);
    }
    double elapsed_put = get_time_sec() - start;
    printf("[PUT] Total: %.4f s | Throughput: %.0f ops/sec | Latency: %.2f ns/op\n",
           elapsed_put, NUM_OPERATIONS / elapsed_put, (elapsed_put / NUM_OPERATIONS) * 1e9);

    // 2. Benchmark Hits (Lookup existing keys)
    start = get_time_sec();
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        snprintf(key_buf, sizeof(key_buf), "key_%d", i);
        ht_get(ht, key_buf);
    }
    double elapsed_get_hit = get_time_sec() - start;
    printf("[GET HIT] Total: %.4f s | Throughput: %.0f ops/sec | Latency: %.2f ns/op\n",
           elapsed_get_hit, NUM_OPERATIONS / elapsed_get_hit, (elapsed_get_hit / NUM_OPERATIONS) * 1e9);

    // 3. Benchmark Misses (Lookup non-existent keys)
    start = get_time_sec();
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        snprintf(key_buf, sizeof(key_buf), "missing_%d", i);
        ht_get(ht, key_buf);
    }
    double elapsed_get_miss = get_time_sec() - start;
    printf("[GET MISS] Total: %.4f s | Throughput: %.0f ops/sec | Latency: %.2f ns/op\n",
           elapsed_get_miss, NUM_OPERATIONS / elapsed_get_miss, (elapsed_get_miss / NUM_OPERATIONS) * 1e9);

    ht_destroy(ht);
    return 0;
}
