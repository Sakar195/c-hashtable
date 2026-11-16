#include "hashtable.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

void test_basic_crud(void) {
    printf("[RUN] Testing Basic CRUD & Updates...\n");
    ht_t *ht = ht_create();
    assert(ht != NULL);
    assert(ht_count(ht) == 0);

    // Insert
    assert(ht_put(ht, "user:1", "Alice") == true);
    assert(ht_put(ht, "user:2", "Bob") == true);
    assert(ht_count(ht) == 2);

    // Retrieve
    assert(strcmp(ht_get(ht, "user:1"), "Alice") == 0);
    assert(strcmp(ht_get(ht, "user:2"), "Bob") == 0);
    assert(ht_get(ht, "user:99") == NULL);

    // Update
    assert(ht_put(ht, "user:1", "Alicia") == true);
    assert(ht_count(ht) == 2); // Count must not increase on update!
    assert(strcmp(ht_get(ht, "user:1"), "Alicia") == 0);

    ht_destroy(ht);
    printf("[PASS] Basic CRUD & Updates\n");
}

void test_collision_and_tombstones(void) {
    printf("[RUN] Testing Tombstones and Linear Probing Chains...\n");
    ht_t *ht = ht_create();

    // Insert keys
    assert(ht_put(ht, "alpha", "1") == true);
    assert(ht_put(ht, "beta", "2") == true);
    assert(ht_put(ht, "gamma", "3") == true);

    // Delete middle key (creates a TOMBSTONE)
    assert(ht_delete(ht, "beta") == true);
    assert(ht_count(ht) == 2);
    assert(ht_get(ht, "beta") == NULL);

    // Verify key after the tombstone is still reachable!
    assert(ht_get(ht, "gamma") != NULL);
    assert(strcmp(ht_get(ht, "gamma"), "3") == 0);

    // Deleting non-existent key returns false
    assert(ht_delete(ht, "non_existent") == false);

    ht_destroy(ht);
    printf("[PASS] Tombstones and Linear Probing Chains\n");
}

void test_growth_and_rehash(void) {
    printf("[RUN] Testing Dynamic Resizing & Rehashing (2,000 items)...\n");
    ht_t *ht = ht_create();

    char key_buf[32];
    char val_buf[32];

    // Insert 2,000 items (forces resize: 16 -> 32 -> 64 -> 128 -> ... -> 4096)
    for (int i = 0; i < 2000; i++) {
        snprintf(key_buf, sizeof(key_buf), "key_%d", i);
        snprintf(val_buf, sizeof(val_buf), "val_%d", i);
        assert(ht_put(ht, key_buf, val_buf) == true);
    }

    assert(ht_count(ht) == 2000);

    // Verify EVERY single key survived the rehash without corruption
    for (int i = 0; i < 2000; i++) {
        snprintf(key_buf, sizeof(key_buf), "key_%d", i);
        snprintf(val_buf, sizeof(val_buf), "val_%d", i);
        const char *val = ht_get(ht, key_buf);
        assert(val != NULL);
        assert(strcmp(val, val_buf) == 0);
    }

    ht_destroy(ht);
    printf("[PASS] Dynamic Resizing & Rehashing\n");
}

void test_null_safety(void) {
    printf("[RUN] Testing Null Safety Guard Clauses...\n");
    assert(ht_get(NULL, "key") == NULL);
    assert(ht_put(NULL, "key", "val") == false);
    assert(ht_delete(NULL, "key") == false);
    assert(ht_count(NULL) == 0);
    ht_destroy(NULL); // Must not segfault
    printf("[PASS] Null Safety\n");
}

int main(void) {
    printf("=== RUNNING HASH TABLE TEST SUITE ===\n");
    test_basic_crud();
    test_collision_and_tombstones();
    test_growth_and_rehash();
    test_null_safety();
    printf("=== ALL TESTS PASSED SUCCESSFULLY! ===\n");
    return 0;
}
