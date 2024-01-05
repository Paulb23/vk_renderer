#ifndef ENGINE_H_
#define ENGINE_H_

#include <bits/stdint-intn.h>
#include "src/vulkan/vk_window.h"
#include "src/vulkan/vk_renderer.h"
#include "src/camera.h"
#include "src/object.h"

typedef struct Engine {
    int32_t max_ticks;
    double ns;

    int32_t uptime;
    int32_t frames;

    Window window;
    VkRenderer renderer;

    Camera camera;
    Vector objects;
} Engine;

Engine *engine_create(size_t p_width, size_t p_height);

void engine_add_object(Engine *p_engine, Object *p_object);

void engine_run(Engine *p_engine);

void engine_cleanup(Engine *p_engine);

#endif
