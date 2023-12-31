#ifndef MEMORY_H_
#define MEMORY_H_

#include <stdlib.h>

void *mmalloc(size_t p_size);

void *mrealloc(void *p_ptr, size_t p_new_size);

void mfree(void *p_ptr);

#endif
