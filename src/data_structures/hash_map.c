#include "hash_map.h"

#include "src/io/memory.h"
#include "src/error/error.h"
#include "vector.h"

typedef struct MapEntry {
    char *key;
    void *data;
} MapEntry;

struct HashMap {
    size_t key_size;
    size_t value_size;
    Vector map;
};

HashMap *hashmap_create(size_t value_size) {
    HashMap *map = mmalloc(sizeof(HashMap));
    map->value_size = value_size;
    map->map = (Vector){ 0, 0, sizeof(MapEntry), NULL };
    return map;
}

void hashmap_free(HashMap *r_hashmap) {
    for (size_t i = 0; i < r_hashmap->map.size; i++) {
        const MapEntry *entry = r_hashmap->map.data + (i * r_hashmap->map.data_size);
        mfree(entry->key);
        mfree(entry->data);
    }
}

void hashmap_insert(HashMap *r_hashmap, const char *p_key, size_t p_key_size, const void *p_data) {
    for (size_t i = 0; i < r_hashmap->map.size; i++) {
        const MapEntry *entry = r_hashmap->map.data + (i * r_hashmap->map.data_size);
        if (strncmp(p_key, entry->key, p_key_size) == 0) {
            memcpy(entry->data, p_data, r_hashmap->value_size);
            return;
        }
    }

    // Ugly but will do...
    MapEntry *new_entry = mmalloc(sizeof(MapEntry));
    new_entry->key = mmalloc(p_key_size);
    new_entry->data = mmalloc(r_hashmap->value_size);

    memcpy(new_entry->key, p_key, p_key_size);
    memcpy(new_entry->data, p_data, r_hashmap->value_size);
    vector_push_back(&r_hashmap->map, new_entry);

    mfree(new_entry);
}

void *hashmap_get(HashMap *r_hashmap, const char *p_key, size_t p_key_size) {
    for (size_t i = 0; i < r_hashmap->map.size; i++) {
        const MapEntry *entry = r_hashmap->map.data + (i * r_hashmap->map.data_size);
        if (strncmp(p_key, entry->key, p_key_size) == 0) {
            return entry->data;
        }
    }
    return NULL;
}
