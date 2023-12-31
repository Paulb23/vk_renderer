#include "vectors.h"
#include <math.h>

Vect3 vect3_normalize(const Vect3 p_a) {
    const float v = sqrt(p_a.x * p_a.x + p_a.y * p_a.y + p_a.z * p_a.z);
    return (Vect3) {
        p_a.x / v,
        p_a.y / v,
        p_a.z / v
    };
}

float vect3_dot(const Vect3 p_a, const Vect3 p_b) {
    return p_a.x * p_b.x + p_a.y * p_b.y + p_a.z * p_b.z;
}

Vect3 vect3_cross(const Vect3 p_a, const Vect3 p_b) {
    return (Vect3) {
        p_a.y * p_b.z - p_a.z * p_b.y,
        p_a.z * p_b.x - p_a.x * p_b.z,
        p_a.x * p_b.y - p_a.y * p_b.x
    };
}

Vect3 vect3_add(const Vect3 p_a, const Vect3 p_b) {
    return (Vect3) {
        p_a.x + p_b.x,
        p_a.y + p_b.y,
        p_a.z + p_b.z
    };
}

Vect3 vect3_sub(const Vect3 p_a, const Vect3 p_b) {
    return (Vect3) {
        p_a.x - p_b.x,
        p_a.y - p_b.y,
        p_a.z - p_b.z
    };
}

Vect3 vect3_multi(const Vect3 p_a, const float p_factor) {
    return (Vect3) {
        p_a.x * p_factor,
        p_a.y * p_factor,
        p_a.z * p_factor
    };
}
