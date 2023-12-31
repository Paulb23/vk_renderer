#ifndef HASH_MAP_H_
#define HASH_MAP_H_

#include <stdlib.h>

typedef struct HashMap HashMap;

HashMap *hashmap_create(size_t value_size);

void hashmap_free(HashMap *r_hashmap);

void hashmap_insert(HashMap *r_hashmap, const char *p_key, size_t p_key_size, const void *p_data);

void *hashmap_get(HashMap *r_hashmap, const char *p_key, size_t p_key_size);

#endif
