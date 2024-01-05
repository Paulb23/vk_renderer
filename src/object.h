#ifndef OBJECT_H_
#define OBJECT_H_

#include "src/data_structures/vector.h"
#include "src/vulkan/vk_renderer.h"

typedef struct Object {
    Vect3 position;
    Vect3 rotation;
    Surface surface;
} Object;

#endif

void object_get_bias(const Object *p_object, Mat4 r_bias);
