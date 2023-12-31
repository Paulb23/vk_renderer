#include "vector.h"

#include <stdlib.h>

#include "src/io/memory.h"
#include "src/error/error.h"

void vector_set(Vector *r_vector, const size_t p_idx, const void *p_data) {
    ERR_FAIL_UINDEX(p_idx, r_vector->size);
    memcpy(r_vector->data + (p_idx * r_vector->data_size), p_data, r_vector->data_size);
}

void vector_push_back(Vector *r_vector, const void *p_data) {
    if (r_vector->size == r_vector->capacity) {
        size_t new_size = r_vector->capacity * 2;
        if (r_vector->capacity == 0) {
            new_size = 1;
        }
        r_vector->data = mrealloc(r_vector->data, r_vector->data_size * new_size);
        r_vector->capacity = new_size;
    }

    vector_set(r_vector, r_vector->size++, p_data);
}

void vector_free(Vector *r_vector) {
    r_vector->capacity = 0;
    r_vector->size = 0;
    mfree(r_vector->data);
}
