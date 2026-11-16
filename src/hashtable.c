#include "hashtable.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * Initial capacity of the hash table bucket array.
 * Must be a power of 2.
 *
 * Hardware/Systems rationale:
 * 1. Power of 2 allows replacing the expensive CPU division instruction
 *    (`hash % capacity`, ~15-40 CPU cycles) with an ultra-fast bitwise AND
 *    (`hash & (capacity - 1)`, 1 CPU cycle).
 * 2. At 24 bytes per entry, 16 entries occupy exactly 384 bytes (~6 CPU cache
 * lines), keeping initial memory footprint small while avoiding premature
 * resizing.
 */
#define INITIAL_CAPACITY 16

/**
 * 64-bit FNV-1a Offset Basis (starting hash seed).
 *
 * Systems rationale:
 * A non-zero mathematically calculated seed. Starting with a non-zero value
 * ensures that empty strings or strings starting with null/zero bytes do not
 * produce a hash of 0, preserving distribution across the hash space.
 *
 * The 'ULL' suffix enforces an 'unsigned long long' literal (guaranteed
 * 64-bit), preventing 32-bit truncation warnings on different architectures.
 */
#define FNV_OFFSET 0xcbf29ce484222325ULL

/**
 * 64-bit FNV-1a Prime Multiplier.
 *
 * Hardware/Systems rationale:
 * The core "mixing" engine of the hash function. Multiplying a 64-bit register
 * by this prime shifts and scatters bits across the entire register.
 * Its binary representation has an optimal distribution of set bits (1s),
 * maximizing the "avalanche effect" (changing a single bit in the key
 * flips ~50% of the bits in the output hash) using just a single CPU multiply
 * instruction.
 *
 * Note: In C, unsigned integer multiplication is guaranteed by the standard
 * to wrap modulo 2^64 on overflow without causing undefined behavior.
 */
#define FNV_PRIME 0x100000001b3ULL

/**
 * Maximum Load Factor threshold before triggering an array resize (Rehash).
 *
 * Algorithmic & Hardware Rationale (Why 70%?):
 * 1. Primary Clustering: In Open Addressing with Linear Probing, occupied slots
 *    tend to group together. The probability of hitting an existing cluster
 *    increases with cluster size, creating a "snowball effect" of collisions.
 *
 * 2. Mathematical Proof (Knuth's Probe Formula):
 *    Average probes for insertion/miss: E(probes) ≈ 0.5 * (1 + (1 / (1 - α)^2))
 *    - At α = 0.50 (50% full): ~2.5 average probes.
 *    - At α = 0.70 (70% full): ~6.0 average probes.
 *    - At α = 0.80 (80% full): ~13.0 average probes (probe chain doubles!).
 *    - At α = 0.90 (90% full): ~50.5 average probes (falls off a cliff).
 *
 * 3. Mechanical Sympathy (CPU Cache Lines):
 *    Each entry is 24 bytes; a standard L1 cache line is 64 bytes (~2.6
 * entries). At a 70% load factor, average probe length (3 to 6 entries =
 * ~72-144 bytes) spans only 1 to 3 contiguous cache lines. The CPU hardware
 * prefetcher absorbs this with almost zero latency in L1/L2 SRAM. Beyond 70%,
 * probe chains blow past 10+ cache lines, thrashing L1 cache and degrading O(1)
 * lookups into an O(N) linear scan across RAM.
 *
 * 4. Space/Time Tradeoff:
 *    Unlike Java's Separate Chaining (which uses 0.75 without clustering),
 *    0.70 is the optimal compromise for Linear Probing between memory
 *    utilization (70% of RAM utilized) and CPU cache locality.
 */
#define MAX_LOAD_FACTOR 0.70

// Internal Data Structures
typedef enum {
  BUCKET_EMPTY = 0, // O calloc to automatically initialize to EMPTY
  BUCKET_OCCUPIED,
  BUCKET_TOMBSTONE
} bucket_state_t;

typedef struct {
  char *key;
  char *value;
  bucket_state_t state;
} entry_t;

struct ht {
  size_t count;     // Number of active items
  size_t capacity;  // Total allocated buckets
  entry_t *entries; // Array of buckets
};

// Internal Helper function
static uint64_t hash_key(const char *key) {
  uint64_t hash = FNV_OFFSET;
  while (*key) {
    hash ^= (uint8_t)(*key);
    hash *= FNV_PRIME;
    key++;
  }
  return hash;
}

static bool ht_resize(ht_t *table, size_t new_capacity) {
  if (table == NULL)
    return false;
  entry_t *entryArray = calloc(new_capacity, sizeof(entry_t));
  if (entryArray == NULL) {
    return false;
  }

  entry_t *old_entries = table->entries;
  size_t old_capacity = table->capacity;

  table->entries = entryArray;
  table->capacity = new_capacity;
  table->count = 0;
  for (size_t i = 0; i < old_capacity; i++) {
    if (old_entries[i].state == BUCKET_OCCUPIED) {
      uint64_t hashed_key = hash_key(old_entries[i].key);
      size_t new_index = hashed_key % table->capacity;

      // Probe until we find an empty slot in the new table:
      while (table->entries[new_index].state == BUCKET_OCCUPIED) {
        new_index = (new_index + 1) % table->capacity;
      }

      table->entries[new_index].key = old_entries[i].key;
      table->entries[new_index].value = old_entries[i].value;
      table->entries[new_index].state = BUCKET_OCCUPIED;
      table->count++;
    }
  }
  free(old_entries);
  return true;
}

// Public API Implementation

ht_t *ht_create(void) {
  ht_t *hashtable = malloc(sizeof(struct ht));
  if (hashtable == NULL)
    return NULL;
  // Use calloc() instead of malloc() to guarantee all bits are zeroed.
  // Mechanically, this initializes every bucket's state to BUCKET_EMPTY (enum
  // 0) and all internal string pointers to NULL in a single call, preventing
  // undefined behavior from reading garbage heap memory.
  entry_t *entryArray = calloc(INITIAL_CAPACITY, sizeof(entry_t));
  if (entryArray == NULL) {
    free(hashtable);
    return NULL;
  }

  hashtable->capacity = INITIAL_CAPACITY;
  hashtable->count = 0;
  hashtable->entries = entryArray;
  return hashtable;
}

void ht_destroy(ht_t *table) {
  if (table == NULL)
    return;
  for (size_t i = 0; i < table->capacity; i++) {
    entry_t *entry = &table->entries[i];
    if (entry->state == BUCKET_OCCUPIED) {
      free(entry->key);
      free(entry->value);
    }
  }
  free(table->entries);
  free(table);
}

bool ht_put(ht_t *table, const char *key, const char *value) {
  if (table == NULL || key == NULL || value == NULL)
    return false;
  // If loadfactor >= 0.7 (70%), double the capacity:
  if ((double)table->count / table->capacity >= MAX_LOAD_FACTOR) {
    if (!ht_resize(table, table->capacity * 2)) {
      return false; // Out of memory
    }
  }
  uint64_t hashedKey = hash_key(key);
  size_t index = hashedKey % table->capacity;
  while (true) {
    if (table->entries[index].state == BUCKET_EMPTY) {
      table->entries[index].key = strdup(key);
      table->entries[index].value = strdup(value);
      table->entries[index].state = BUCKET_OCCUPIED;
      table->count++;
      return true;
    } else if (table->entries[index].state == BUCKET_OCCUPIED) {
      if (strcmp(table->entries[index].key, key) == 0) {
        free(table->entries[index].value);
        table->entries[index].value = strdup(value);
        return true;
      }
    }
    index = (index + 1) % table->capacity;
  }
  return false;
}

const char *ht_get(ht_t *table, const char *key) {
  if (table == NULL || key == NULL)
    return NULL;

  uint64_t hashedKey = hash_key(key);
  size_t index = hashedKey % table->capacity;
  while (true) {
    if (table->entries[index].state == BUCKET_EMPTY)
      return NULL;
    else if (table->entries[index].state == BUCKET_OCCUPIED) {
      if (strcmp(table->entries[index].key, key) == 0) {
        return table->entries[index].value;
      }
    }
    // If keys didn't match, or if bucket is a TOMBSTONE, probe forward:
    index = (index + 1) % table->capacity;
  }
  return NULL;
}

bool ht_delete(ht_t *table, const char *key) {
  if (table == NULL || key == NULL)
    return false;

  uint64_t hashedKey = hash_key(key);
  size_t index = hashedKey % table->capacity;

  while (true) {
    if (table->entries[index].state == BUCKET_EMPTY)
      return false;
    else if (table->entries[index].state == BUCKET_OCCUPIED) {
      if (strcmp(table->entries[index].key, key) == 0) {
        // delete
        free(table->entries[index].key);
        free(table->entries[index].value);
        // to avoid dangling pointers
        table->entries[index].key = NULL;
        table->entries[index].value = NULL;
        table->entries[index].state = BUCKET_TOMBSTONE;
        table->count--;
        return true;
      }
    }
    // If keys didn't match, or if bucket is a TOMBSTONE, probe forward:
    index = (index + 1) % table->capacity;
  }
  return false;
}

size_t ht_count(ht_t *table) {
  if (!table)
    return 0;
  return table->count;
}
