#include "src/io/io.h"
#include "src/vulkan/vk_window.h"
#include "src/vulkan/vk_renderer.h"

#include "src/engine.h"
#include "src/camera.h"
#include "src/object.h"
#include "src/data_structures/vector.h"
#include "src/math/vectors.h"

int main(void) {
    Engine *engine = engine_create(800, 600);

    // Load model data
    char model_path[512];
    get_resource_path(model_path, "resources/viking_room.obj");

    Vector vertexes;
    Vector indices;
    load_obj(model_path, &vertexes, &indices);

    char image_path[512];
    get_resource_path(image_path, "resources/viking_room.png");
    Texture *texture = texture_create(&engine->renderer, &engine->window, image_path);

    for (int i = 0; i < 15; i++) {
        for (int j = 0; j < 15; j++) {
            engine_add_object(engine, &(Object) {
                .position = (Vect3){(3 * i), (3 * j), 0},
                .rotation = (Vect3){-90, 0, 0},
                .surface = *surface_create(&engine->renderer, &engine->window, vertexes, indices, texture),
            });
        }
    }

    engine_run(engine);
    engine_cleanup(engine);
    return 0;
}
