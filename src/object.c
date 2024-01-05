#include "object.h"

#include "src/math/angles.h"

void object_get_bias(const Object *p_object, Mat4 r_bias) {
    memcpy(r_bias,
    (Mat4) {{1, 0, 0, 0},
            {0, 1, 0, 0},
            {0, 0, 1, 0},
            {0, 0, 0, 1}}
    , sizeof(Mat4));

    mat4_rotate(r_bias,  degtorad(p_object->rotation.x), (Vect3){1, 0, 0});
    mat4_rotate(r_bias,  degtorad(p_object->rotation.y), (Vect3){0, 1, 0});
    mat4_rotate(r_bias,  degtorad(p_object->rotation.z), (Vect3){0, 0, 1});
    mat4_translate(r_bias, p_object->position);
}
