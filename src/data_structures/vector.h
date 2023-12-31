#ifndef VECTOR_H_
#define VECTOR_H_

#include <stdlib.h>

typedef struct Vector {
    size_t capacity;
    size_t size;
    size_t data_size;
    void *data;
} Vector;

void vector_set(Vector *r_vector, const size_t p_idx, const void *p_data);

void vector_push_back(Vector *r_vector, const void *p_data);

void vector_free(Vector *r_vector);

#endif
