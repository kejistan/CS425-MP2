#ifndef MAP_H
#define MAP_H

#include <stdint.h>

typedef size_t (*hash_function_t)(int32_t);

typedef struct hash_bucket
{
	int32_t key;
	void *value;
	struct hash_bucket *next;
} hash_bucket_t;

typedef struct hash_table
{
	hash_function_t hash;
	size_t table_size;
	hash_bucket_t **table;
} hash_table_t;

hash_table_t *new_hash_table(hash_function_t hash, size_t table_size);
void free_hash_table(hash_table_t *map);

void hash_insert(hash_table_t *map, int32_t key, void *value);
void hash_remove(hash_table_t *map, int32_t key);
void *hash_find(hash_table_t *map, int32_t key);

#endif // MAP_H
