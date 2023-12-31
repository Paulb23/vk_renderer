#include "memory.h"

#include <stdlib.h>

#include "src/error/error.h"

void *mmalloc(size_t p_size) {
    void *ptr = malloc(p_size);
    CRASH_NULL_MSG(ptr, "%s", "Out of memory!");
    return ptr;
}

void *mrealloc(void *p_ptr, size_t p_new_size) {
    void *ptr = realloc(p_ptr, p_new_size);
    CRASH_NULL_MSG(ptr, "%s", "Out of memory!");
    return ptr;
}

void mfree(void *p_ptr) {
    free(p_ptr);
}
