#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "map.h"

hash_table_t *new_hash_table(hash_function_t hash, size_t table_size)
{
	hash_table_t *map = malloc(sizeof(hash_table_t));
	map->hash = hash;
	map->table_size = table_size = sizeof(hash_bucket_t *) * table_size;
	map->table = malloc(map->table_size);
	memset(map->table, 0, map->table_size);

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

void hash_insert(hash_table_t *map, int32_t key, void *value)
{
	assert(map);
	assert(map->hash);
	assert(map->table);

	size_t table_index = (*map->hash)(key);

	hash_bucket_t *store = malloc(sizeof(hash_bucket_t));
	store->key = key;
	store->value = value;
	store->next = map->table[table_index];
	map->table[table_index] = store;
}

void hash_remove(hash_table_t *map, int32_t key)
{
	assert(map);
	assert(map->hash);
	assert(map->table);

	size_t table_index = (*map->hash)(key);

	hash_bucket_t *bucket = map->table[table_index];
	map->table[table_index] = bucket->next;

	free(bucket->value);
	free(bucket);
}

void *hash_find(hash_table_t *map, int32_t key)
{
	assert(map);
	assert(map->hash);
	assert(map->table);

	size_t table_index = (*map->hash)(key);

	hash_bucket_t *store = map->table[table_index];
	while (store && store->key != key) {
		store = store->next;
	}

	if (!store) return NULL;

	return store->value;
}
