#ifndef MATRICES_H_
#define MATRICES_H_

#include "vectors.h"

typedef float Mat4[4][4];

void mat4_multi(Mat4 r_mat, const Mat4 p_b);

void mat4_rotate(Mat4 r_mat, const float p_radians, const Vect3 p_axis);

void mat4_translate(Mat4 r_mat, const Vect3 p_v);

void mat4_look_at(Mat4 r_mat, const Vect3 p_camera_pos, const Vect3 p_position, const Vect3 p_up);

void mat4_perspective(Mat4 r_mat, const float p_fov_y_radians, const float p_aspect, const float p_znear, const float p_zfar);

#endif
