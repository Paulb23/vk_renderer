#include "matrices.h"

#include <memory.h>
#include <math.h>

void mat4_multi(Mat4 r_mat, const Mat4 p_b) {
    Mat4 ret_mtx;
    memcpy(ret_mtx, r_mat, sizeof(Mat4));

    for (int i = 0; i < 4; i++) {
        ret_mtx[i][0] = (r_mat[i][0] * p_b[0][0]) +
                        (r_mat[i][1] * p_b[1][0]) +
                        (r_mat[i][2] * p_b[2][0]) +
                        (r_mat[i][3] * p_b[3][0]);

        ret_mtx[i][1] = (r_mat[i][0] * p_b[0][1]) +
                        (r_mat[i][1] * p_b[1][1]) +
                        (r_mat[i][2] * p_b[2][1]) +
                        (r_mat[i][3] * p_b[3][1]);

        ret_mtx[i][2] = (r_mat[i][0] * p_b[0][2]) +
                        (r_mat[i][1] * p_b[1][2]) +
                        (r_mat[i][2] * p_b[2][2]) +
                        (r_mat[i][3] * p_b[3][2]);

        ret_mtx[i][3] = (r_mat[i][0] * p_b[0][3]) +
                        (r_mat[i][1] * p_b[1][3]) +
                        (r_mat[i][2] * p_b[2][3]) +
                        (r_mat[i][3] * p_b[3][3]);
    }

    memcpy(r_mat, ret_mtx, sizeof(Mat4));
}

void mat4_rotate(Mat4 r_mat, const float p_radians, const Vect3 p_axis) {
    const float c = cos(p_radians);
    const float s = sin(p_radians);

    const Vect3 axis = vect3_normalize(p_axis);
    const Vect3 temp = {
        (1 - c) * axis.x,
        (1 - c) * axis.y,
        (1 - c) * axis.z
    };

    // Calulate rotation matrix
    Mat4 rotation_mtx = {
        { c + temp.x * axis.x,            temp.x * axis.y + s * axis.z,   temp.x * axis.z - s * axis.y, 0 },
        { temp.y * axis.x - s * axis.z,   c + temp.y * axis.y,            temp.y * axis.z + s * axis.x, 0 },
        { temp.z * axis.x + s * axis.y,   temp.z * axis.y - s * axis.x,   c + temp.z * axis.z,          0 },
        { 0,                              0,                              0,                            1 },
    };

    // Apply rotation matrix
    mat4_multi(r_mat, rotation_mtx);
}

void mat4_translate(Mat4 r_mat, const Vect3 p_v) {
    Mat4 translation_mtx;
    memcpy(translation_mtx, r_mat, sizeof(Mat4));

    translation_mtx[3][0] = r_mat[0][0] * p_v.x + r_mat[1][0] * p_v.y + r_mat[2][0] * p_v.z + r_mat[3][0];
    translation_mtx[3][1] = r_mat[0][1] * p_v.x + r_mat[1][1] * p_v.y + r_mat[2][1] * p_v.z + r_mat[3][1];
    translation_mtx[3][2] = r_mat[0][2] * p_v.x + r_mat[1][2] * p_v.y + r_mat[2][2] * p_v.z + r_mat[3][2];
    translation_mtx[3][3] = r_mat[0][3] * p_v.x + r_mat[1][3] * p_v.y + r_mat[2][3] * p_v.z + r_mat[3][3];

    memcpy(r_mat, translation_mtx, sizeof(Mat4));
}

void mat4_look_at(Mat4 r_mat, const Vect3 p_camera_pos, const Vect3 p_position, const Vect3 p_up) {
    const Vect3 f = vect3_normalize(vect3_sub(p_position, p_camera_pos));
    const Vect3 s = vect3_normalize(vect3_cross(p_up, f));
    const Vect3 u = vect3_cross(f, s);

    r_mat[0][0] = s.x;
    r_mat[1][0] = s.y;
    r_mat[2][0] = s.z;

    r_mat[0][1] = u.x;
    r_mat[1][1] = u.y;
    r_mat[2][1] = u.z;

    r_mat[0][2] = f.x;
    r_mat[1][2] = f.y;
    r_mat[2][2] = f.z;

    r_mat[3][0] = vect3_dot(s, p_camera_pos);
    r_mat[3][1] = vect3_dot(u, p_camera_pos);
    r_mat[3][2] = vect3_dot(f, p_camera_pos);
}

void mat4_perspective(Mat4 r_mat, const float p_fov_y_radians, const float p_aspect, const float p_znear, const float p_zfar) {
    const float tan_half_angle = tan(p_fov_y_radians / 2);

    r_mat[0][0] = 1 / (p_aspect * tan_half_angle);
    r_mat[1][1] = 1 / tan_half_angle;
    r_mat[2][2] = -(p_zfar + p_znear) / (p_zfar - p_znear);
    r_mat[2][3] = -1;
    r_mat[3][2] = -(2 * p_zfar * p_znear) / (p_zfar - p_znear);
}
