#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdbool.h>
#include <stddef.h>

// Opaque handle to the hash table. 
// The actual struct layout is hidden in hashtable.c.
typedef struct ht ht_t;

/**
 * @brief Allocates and initializes a new hash table.
 * @return Pointer to the new hash table, or NULL on memory allocation failure.
 */
ht_t* ht_create(void);

/**
 * @brief Destroys the hash table, freeing all internal memory, 
 *        including duplicated keys and values.
 * @param table The hash table to destroy. If NULL, does nothing.
 */
void ht_destroy(ht_t* table);

/**
 * @brief Inserts a key-value pair into the hash table.
 *        If the key already exists, the old value is freed and replaced.
 *        The hash table makes internal copies of both 'key' and 'value'.
 * @param table The hash table.
 * @param key The string key.
 * @param value The string value.
 * @return true if successful, false if memory allocation failed.
 */
bool ht_put(ht_t* table, const char* key, const char* value);

/**
 * @brief Retrieves a value associated with a key.
 * @param table The hash table.
 * @param key The string key to search for.
 * @return Pointer to the value string, or NULL if the key is not found.
 *         The returned pointer is owned by the hash table and must not be freed by the caller.
 */
const char* ht_get(ht_t* table, const char* key);

/**
 * @brief Removes a key-value pair from the hash table.
 *        Places a TOMBSTONE to preserve linear probing chains.
 * @param table The hash table.
 * @param key The string key to delete.
 * @return true if the key was found and deleted, false if the key was not found.
 */
bool ht_delete(ht_t* table, const char* key);

/**
 * @brief Returns the number of active items in the hash table.
 */
size_t ht_count(ht_t* table);

#endif // HASHTABLE_H
