#include "engine.h"

#include <stdbool.h>
#include <SDL2/SDL.h>
#include <vulkan/vulkan.h>
#include <SDL2/SDL_vulkan.h>
#include "src/io/memory.h"
#include "src/error/error.h"


Engine *engine_create(size_t p_width, size_t p_height) {
    CRASH_COND_MSG(SDL_Init(SDL_INIT_EVERYTHING) != 0, "%s", "FATAL: Could not start SDL 2!");
    CRASH_COND_MSG(SDL_Vulkan_LoadLibrary(NULL) != 0, "%s", "FATAL: Could not start SDL 2 Vulkan!");

    Engine *engine = mmalloc(sizeof(Engine));

    engine->max_ticks = 60;
    engine->ns = 0;
    engine->uptime = 0;
    engine->frames = 0;

    vk_window_create(&engine->window, "Toy Vk Renderer", p_width, p_height);
    vk_renderer_create(&engine->renderer, &engine->window, 2);
    camera_init(&engine->camera);
    engine->objects = (Vector) {0, 0, sizeof(Object), NULL};

    return engine;
}

void engine_add_object(Engine *p_engine, Object *p_object) {
    // TODO: Support named entities
    vector_push_back(&p_engine->objects, p_object);
}

void engine_run(Engine *p_engine) {

    // Main loop
    SDL_Event event;

    Uint32 lastTime = SDL_GetTicks();
    p_engine->ns = 1000.0 / p_engine->max_ticks;
    Uint32 timer = SDL_GetTicks();
    double delta = 0;
    int32_t fps = 0;
    int32_t tick = 0;
    p_engine->uptime = 0;

    bool mouse_capture = false;
    bool running = true;
    while (running) {
        Uint32 now = SDL_GetTicks();
        delta += (now - lastTime) / p_engine->ns;
        lastTime = now;

        while (delta >= 1) {

            while(SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    running = false;
                    break;
                }

                if (camera_event(&p_engine->camera, event) && mouse_capture) {
                    SDL_WarpMouseInWindow(p_engine->window.sdl_window, p_engine->window.vk_extent2D.width / 2, p_engine->window.vk_extent2D.height / 2);
                }

                if (event.type == SDL_MOUSEBUTTONDOWN) {
                    mouse_capture = true;
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                }

                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    mouse_capture = false;
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                }

                // Event
            }

            // Update
            camera_physics_process(&p_engine->camera, delta);

            tick++;
            delta--;
        }
        fps++;

        vk_draw_frame(&p_engine->renderer, &p_engine->window, &p_engine->camera, &p_engine->objects);

        if (SDL_GetTicks() - timer > 1000) {
            timer += 1000;
            p_engine->uptime++;
            p_engine->frames = fps;
            fps = 0;
            tick = 0;
        }
    }
}

void engine_cleanup(Engine *p_engine) {
    mfree(p_engine);
}
