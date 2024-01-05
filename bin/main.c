#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <SDL2/SDL.h>
#include <vulkan/vulkan.h>
#include <SDL2/SDL_vulkan.h>

#include "src/error/error.h"
#include "src/io/io.h"
#include "src/vulkan/vk_window.h"
#include "src/vulkan/vk_renderer.h"

#include "src/camera.h"
#include "src/object.h"
#include "src/io/memory.h"
#include "src/data_structures/hash_map.h"
#include "src/data_structures/vector.h"
#include "src/math/vectors.h"
#include "src/math/matrices.h"
#include "src/math/angles.h"

static int32_t max_ticks = 60;
static double ns = 0;

static int32_t uptime = 0;
static int32_t frames = 0;

static uint32_t window_width = 800;
static uint32_t window_height = 600;

int main(void) {
    // Load model data
    char model_path[512];
    get_resource_path(model_path, "resources/viking_room.obj");

    Vector vertexes;
    Vector indices;
    load_obj(model_path, &vertexes, &indices);

    // Init SDL
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL 2", "FATAL: Could not start SDL 2!", NULL);
        return -1;
    }

    if (SDL_Vulkan_LoadLibrary(NULL) != 0) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL 2", "FATAL: Could not load Vulkan library!", NULL);
        return -1;
    }

    Window window;
    vk_window_create(&window, "fds", 800, 600);

    VkRenderer renderer;
    vk_renderer_create(&renderer, &window, 1);

    char image_path[512];
    get_resource_path(image_path, "resources/viking_room.png");
    Texture *texture = texture_create(&renderer, &window, image_path);


    Surface *surface = surface_create(&renderer, &window, vertexes, indices, texture);
    Surface *surface2 = surface_create(&renderer, &window, vertexes, indices, texture);

    Vector objects = (Vector) {0, 0, sizeof(Object), NULL};
    vector_push_back(&objects, &(Object) {
        .position = (Vect3){0, 0, 0},
        .rotation = (Vect3){-90, 0, 0},
        .surface = *surface,
    });

   vector_push_back(&objects, &(Object) {
        .position = (Vect3){0, 3, 0},
        .rotation = (Vect3){-90, 0, 0},
        .surface = *surface2,
    });

    // Main loop
    SDL_Event event;

    Uint32 lastTime = SDL_GetTicks();
    ns = 1000.0 / max_ticks;
    Uint32 timer = SDL_GetTicks();
    double delta = 0;
    int32_t fps = 0;
    int32_t tick = 0;
    uptime = 0;

    Camera camera;
    camera_init(&camera);

    bool mouse_capture = false;

    bool running = true;
    while (running) {
        Uint32 now = SDL_GetTicks();
        delta += (now - lastTime) / ns;
        lastTime = now;

        while (delta >= 1) {

            while(SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    running = false;
                    break;
                }

                if (camera_event(&camera, event) && mouse_capture) {
                    SDL_WarpMouseInWindow(window.sdl_window, (int)(window_width / 2), (int)(window_height / 2));
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
            camera_physics_process(&camera, delta);

            tick++;
            delta--;
        }
        fps++;

        vk_draw_frame(&renderer, &window, &camera, &objects);

        if (SDL_GetTicks() - timer > 1000) {
            timer += 1000;
            uptime++;
            frames = fps;
            fps = 0;
            tick = 0;
        }
    }
    return 0;
}
