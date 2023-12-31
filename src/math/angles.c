
#include "angles.h"
#include <math.h>

float degtorad(const float p_degrees) {
    return (p_degrees * M_PI) / 180;
}

float radtodeg(const float p_radians) {
    return (p_radians * 180) / M_PI;
}

Vect4 eular_to_quanterion(float p_x_radians, float p_y_radians, float p_z_radians) {
    const float c1 = cos(p_x_radians);
    const float s1 = sin(p_x_radians);
    const float c2 = cos(p_y_radians);
    const float s2 = sin(p_y_radians);
    const float c3 = cos(p_z_radians);
    const float s3 = sin(p_z_radians);
    const float w  = sqrt(1.0 + c1 * c2 + c1*c3 - s1 * s2 * s3 + c2*c3) / 2.0f;
    const float w4 = 4.0 * w;

    return (Vect4) {
        .x = (c2 * s3 + c1 * s3 + s1 * s2 * c3) / w4,
        .y = (s1 * c2 + s1 * c3 + c1 * s2 * s3) / w4,
        .z = (-s1 * s3 + c1 * s2 * c3 +s2) / w4,
        .w = w,
    };
}
