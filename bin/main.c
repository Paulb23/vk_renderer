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

    // Create Fixed functions Pipelines

    VkViewport vk_viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)window.vk_extent2D.width,
        .height = (float)window.vk_extent2D.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    VkRect2D vk_scissor = {
        .extent = window.vk_extent2D,
        .offset = {0, 0},
    };

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

    float rot = 0;
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

        // Render
        vkWaitForFences(window.vk_device, 1, &renderer.frame_data[renderer.current_frame].render_fence, VK_TRUE, UINT64_MAX);
        vkResetFences(window.vk_device, 1, &renderer.frame_data[renderer.current_frame].render_fence);

        uint32_t imageIndex;
        vkAcquireNextImageKHR(window.vk_device, window.vk_swapchain, UINT64_MAX, renderer.frame_data[renderer.current_frame].image_available, VK_NULL_HANDLE, &imageIndex);

        vkResetCommandBuffer(renderer.frame_data[renderer.current_frame].command_buffer, 0);

        // Record commands
        VkCommandBufferBeginInfo command_buffer_begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = 0,
            .pInheritanceInfo = NULL,
        };

        VkResult error = vkBeginCommandBuffer(renderer.frame_data[renderer.current_frame].command_buffer, &command_buffer_begin_info);
        if (error != VK_SUCCESS) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to start command buffer recording!", NULL);
            running = false;
        }

        VkRenderPassBeginInfo render_pass_info = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = renderer.renderpass,
            .framebuffer = renderer.vk_frame_buffers[imageIndex],
            .renderArea = {
                .offset = {0, 0},
                .extent = window.vk_extent2D,
            },
            .clearValueCount = 2,
            .pClearValues = (VkClearValue[]) {
                {
                    .color = {
                        .float32 = {0.0f, 0.0f, 0.0f, 0.0f},
                    },
                },
                {
                    .depthStencil = {
                        .depth = 1.0f,
                        .stencil = 0,
                    },
                },
            },
        };

        // Record Render Pass
        vkCmdBeginRenderPass(renderer.frame_data[renderer.current_frame].command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(renderer.frame_data[renderer.current_frame].command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer.pipeline);

        vkCmdSetViewport(renderer.frame_data[renderer.current_frame].command_buffer, 0, 1, &vk_viewport);
        vkCmdSetScissor(renderer.frame_data[renderer.current_frame].command_buffer, 0, 1, &vk_scissor);

        vkCmdBindPipeline(renderer.frame_data[renderer.current_frame].command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer.pipeline);

        for (int i = 0; i < objects.size; i++) {
            Object object = *(Object *)vector_get(&objects, i);

            // Create CameraBuffer
            CameraBuffer camera_bufffer = {
                .model = {
                    {1, 0, 0, 0},
                    {0, 1, 0, 0},
                    {0, 0, 1, 0},
                    {0, 0, 0, 1},
                },
                .view = {
                    {1, 0, 0, 0},
                    {0, 1, 0, 0},
                    {0, 0, 1, 0},
                    {0, 0, 0, 1},
                },
                .proj = {
                    {0, 0, 0, 0},
                    {0, 0, 0, 0},
                    {0, 0, 0, 0},
                    {0, 0, 0, 0},
                },
            };

            rot++;
            if (rot > 360) {
                rot = 0;
            }
            mat4_rotate(camera_bufffer.model,  degtorad(object.rotation.x), (Vect3){1, 0, 0});
            mat4_rotate(camera_bufffer.model,  degtorad(object.rotation.y), (Vect3){0, 1, 0});
            mat4_rotate(camera_bufffer.model,  degtorad(object.rotation.z), (Vect3){0, 0, 1});
            mat4_translate(camera_bufffer.model, object.position);

            camera_get_bias(&camera, camera_bufffer.view);

            mat4_perspective(camera_bufffer.proj, degtorad(45), (float)window.vk_extent2D.width / (float)window.vk_extent2D.height, (float)0.1, (float)10.0);
            camera_bufffer.proj[1][1] *= -1;

            SurfaceDescriptorSet s = *(SurfaceDescriptorSet *)vector_get(&object.surface.descriptor_sets, renderer.current_frame);
            memcpy(s.camera_data, &camera_bufffer, sizeof(CameraBuffer));

            VkBuffer vertexBuffers[] = {surface->vertex_buffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(renderer.frame_data[renderer.current_frame].command_buffer, 0, 1, vertexBuffers, offsets);

            vkCmdBindIndexBuffer(renderer.frame_data[renderer.current_frame].command_buffer, surface->index_buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindDescriptorSets(renderer.frame_data[renderer.current_frame].command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer.pipeline_layout, 0, 1, &s.descriptor_set, 0, NULL);

            vkCmdDrawIndexed(renderer.frame_data[renderer.current_frame].command_buffer, surface->index_data.size, 1, 0, 0, 0);
        }

        vkCmdEndRenderPass(renderer.frame_data[renderer.current_frame].command_buffer);
        error = vkEndCommandBuffer(renderer.frame_data[renderer.current_frame].command_buffer);
        if (error != VK_SUCCESS) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to end command buffer recording!", NULL);
            running = false;
        }

        VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = (VkSemaphore[]) {
                renderer.frame_data[renderer.current_frame].image_available,
            },
            .pWaitDstStageMask = (VkPipelineStageFlags[]) {
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
            },
            .commandBufferCount = 1,
            .pCommandBuffers = &renderer.frame_data[renderer.current_frame].command_buffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = (VkSemaphore[]) {
                renderer.frame_data[renderer.current_frame].render_finished,
            },
        };

        error = vkQueueSubmit(window.vk_queue, 1, &submit_info, renderer.frame_data[renderer.current_frame].render_fence);
        if (error != VK_SUCCESS) {
            running = false;
        }

        VkPresentInfoKHR present_info = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = (VkSemaphore[]) {
                renderer.frame_data[renderer.current_frame].render_finished,
            },
            .swapchainCount = 1,
            .pSwapchains = (VkSwapchainKHR[]) {
                window.vk_swapchain,
            },
            .pImageIndices = &imageIndex,
            .pResults = NULL,
        };

        vkQueuePresentKHR(window.vk_queue, &present_info);

        renderer.current_frame = (renderer.current_frame + 1) % renderer.frames;

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
