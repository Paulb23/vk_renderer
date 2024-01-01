#ifndef VK_RENDERER_H_
#define VK_RENDERER_H_

#include <SDL2/SDL.h>
#include <vulkan/vulkan.h>
#include <SDL2/SDL_vulkan.h>
#include "vk_window.h"

typedef struct FrameData {
    VkSemaphore image_available,
    VkSemaphore render_finished;
    VkFence render_fence;
    VkCommandBuffer *command_buffer;
} FrameData;

typedef struct VkRenderer {
    size_t frames;
    FrameData *frame_data;
} VkRenderer;

void kv_renderer_create(VkRenderer *r_vk_renderer, const Window *p_window, size_t p_frame_count);

#endif
