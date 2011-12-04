#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "map.h"

hash_table_t *new_hash_table(hash_function_t hash, size_t table_size)
{
	hash_table_t *map = malloc(sizeof(hash_table_t));
	map->hash = hash;
	map->table_size = table_size;
	map->table = malloc(map->table_size * sizeof(hash_table_t));
	memset(map->table, 0, map->table_size * sizeof(hash_table_t));

	return map;
}

void free_hash_table(hash_table_t *map)
{
	assert(map);

	for (size_t i = 0; i < map->table_size; ++i) {
		hash_bucket_t *store = map->table[i];
		while (store) {
			hash_bucket_t *next = store->next;
			free(store->value);
			free(store);
			store = next;
		}
	}

	free(map->table);
	free(map);
}

void hash_insert(hash_table_t *map, unsigned key[5], void *value)
{
	assert(map);
	assert(map->hash);
	assert(map->table);

	size_t table_index = (*map->hash)(key);

	hash_bucket_t *store = malloc(sizeof(hash_bucket_t));
	memcpy(store->key, key, sizeof(unsigned) * 5);
	store->value = value;
	store->next = map->table[table_index];
	map->table[table_index] = store;
}

/**
 * Removes the item mapped to key from the table
 * @return 0 on success, 1 on not found
 */
int hash_remove(hash_table_t *map, unsigned key[5])
{
	assert(map);
	assert(map->hash);
	assert(map->table);

	size_t table_index = (*map->hash)(key);

	hash_bucket_t *bucket = map->table[table_index];
	hash_bucket_t **prev = &map->table[table_index];
	while (bucket && !(bucket->key[0] == key[0] && bucket->key[1] == key[1] &&
	                   bucket->key[2] == key[2] && bucket->key[3] == key[3] &&
	                   bucket->key[4] == key[4])) {
		prev = &bucket->next;
		bucket = bucket->next;
	}

	if (bucket) {
		*prev = bucket->next;
		free(bucket->value);
		free(bucket);
		return 0;
	}

	return 1;
}

/**
 * Returns the value of the item stored at key in the table
 * @return the value associated with key or NULL if not found
 */
void *hash_find(hash_table_t *map, unsigned key[5])
{
	assert(map);
	assert(map->hash);
	assert(map->table);

	size_t table_index = (*map->hash)(key);

	hash_bucket_t *store = map->table[table_index];
	while (store && !(store->key[0] == key[0] && store->key[1] == key[1] &&
	                  store->key[2] == key[2] && store->key[3] == key[3] &&
	                  store->key[4] == key[4])) {
		store = store->next;
	}

	if (!store) return NULL;

	return store->value;
}
