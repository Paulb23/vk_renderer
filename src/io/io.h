#ifndef IO_H_
#define IO_H_

#include <stddef.h>
#include "src/data_structures/vector.h"

void get_resource_path(char r_dest[512], const char *p_file);

char *read_file(const char *p_path, size_t *r_file_size);

void load_obj(const char *p_path, Vector *r_vertexes, Vector *r_indexes);

#endif
