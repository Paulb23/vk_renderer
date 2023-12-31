#ifndef CAMERA_H_
#define CAMERA_H_

#include <stdbool.h>
#include <SDL2/SDL.h>

#include "src/math/matrices.h"

typedef struct Camera {
    Vect3 position;
    Vect3 rotation;
    float pitch;
    float yaw;
    float sensitivity;
    float move_speed;
} Camera;

void camera_init(Camera *r_camera);

void camera_physics_process(Camera *r_camera, double p_delta);

bool camera_event(Camera *r_camera, SDL_Event p_event);

void camera_get_bias(Camera *r_camera, Mat4 r_bias);

#endif
