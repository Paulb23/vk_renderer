#include "camera.h"

#include "src/error/error.h"
#include "src/math/vectors.h"
#include "src/math/angles.h"
#include "src/error/error.h"

static const float min_y_angle = -50;
static const float max_y_angle = 75;

void camera_init(Camera *r_camera) {
    r_camera->position = (Vect3){-2, 0, -2};
    r_camera->pitch = 0;
    r_camera->yaw = 45;
    r_camera->rotation = (Vect3){0.7f, 0.04f, 0.66f};
    r_camera->sensitivity = 0.1f;
    r_camera->move_speed = 0.1f;
}

void camera_physics_process(Camera *r_camera, double p_delta) {
    const Uint8 *keystates =  SDL_GetKeyboardState(NULL);
    if (keystates[SDL_SCANCODE_W]) {
        r_camera->position = vect3_add(r_camera->position, vect3_multi(r_camera->rotation, r_camera->move_speed * p_delta));
    }
    if (keystates[SDL_SCANCODE_S]) {
        r_camera->position = vect3_sub(r_camera->position, vect3_multi(r_camera->rotation, r_camera->move_speed * p_delta));
    }
    if (keystates[SDL_SCANCODE_A]) {
        r_camera->position = vect3_sub(r_camera->position, vect3_multi(vect3_normalize(vect3_cross(r_camera->rotation, (Vect3){0, 1, 0})), r_camera->move_speed * p_delta));
    }
    if (keystates[SDL_SCANCODE_D]) {
        r_camera->position = vect3_add(r_camera->position, vect3_multi(vect3_normalize(vect3_cross(r_camera->rotation, (Vect3){0, 1, 0})), r_camera->move_speed * p_delta));
    }
}

bool camera_event(Camera *r_camera, SDL_Event p_event) {
    if (p_event.type == SDL_MOUSEMOTION) {
        r_camera->yaw += p_event.motion.xrel * r_camera->sensitivity;
        r_camera->pitch += p_event.motion.yrel * r_camera->sensitivity;

        if(r_camera->pitch > max_y_angle) {
            r_camera->pitch = max_y_angle;
        }
        if(r_camera->pitch < min_y_angle) {
            r_camera->pitch = min_y_angle;
        }

        r_camera->rotation.x = cos(degtorad(r_camera->yaw)) * cos(degtorad(r_camera->pitch));
        r_camera->rotation.y = sin(degtorad(r_camera->pitch));
        r_camera->rotation.z = sin(degtorad(r_camera->yaw)) * cos(degtorad(r_camera->pitch));
        r_camera->rotation = vect3_normalize(r_camera->rotation);
        return true;
    }
    return true;
}

void camera_get_bias(Camera *r_camera, Mat4 r_bias) {
    memcpy(r_bias,
    (Mat4) {{1, 0, 0, 0},
            {0, 1, 0, 0},
            {0, 0, 1, 0},
            {0, 0, 0, 1}}
    , sizeof(Mat4));
    mat4_look_at(r_bias, r_camera->position, vect3_add(r_camera->position, r_camera->rotation), (Vect3){0, 1, 0});
}
