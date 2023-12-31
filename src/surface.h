#ifndef SURFACE_H_
#define SURFACE_H_

#include "math/vectors.h"
#include "data_structures/vector.h"

typedef struct vertex {
    Vect3 pos;
    Vect4 color;
    Vect2 tex_coord;
} Vertex;

typedef struct Surface {
    Vertex vertex_data;
    Vector index_data;
} Surface;

#endif
