#ifndef ANGLES_H_
#define ANGLES_H_

#include "vectors.h"

float degtorad(const float p_degrees);

float radtodeg(const float p_radians);

Vect4 eular_to_quanterion(float p_x_radians, float p_y_radians, float p_z_radians);

#endif
