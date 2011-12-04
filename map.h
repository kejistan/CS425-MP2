#ifndef MAP_H
#define MAP_H

#include <stdint.h>

typedef size_t (*hash_function_t)(unsigned[5]);

typedef struct hash_bucket
{
	unsigned key[5];
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

void hash_insert(hash_table_t *map, unsigned key[5], void *value);
int hash_remove(hash_table_t *map, unsigned key[5]);
void *hash_find(hash_table_t *map, unsigned key[5]);

#endif // MAP_H
