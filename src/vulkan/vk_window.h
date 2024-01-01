#ifndef VK_WINDOW_H_
#define VK_WINDOW_H_

#include <SDL2/SDL.h>
#include <vulkan/vulkan.h>
#include <SDL2/SDL_vulkan.h>

typedef struct WindowImage {
    VkImage vk_image;
    VkImageView vk_image_view;
} WindowImage;

typedef struct Window {
    SDL_Window *sdl_window;

    VkInstance vk_instance;
    VkSurfaceKHR vk_surface;

    VkPhysicalDevice vk_physical_device;
    uint32_t image_count;
    float max_sampler_anisotropy;
    VkSurfaceFormatKHR vk_surface_format;
    VkExtent2D vk_extent2D;
    VkFormat vk_depth_format;

    VkDevice vk_device;
    VkQueue vk_queue;
    uint32_t vk_queue_index;
    VkSwapchainKHR vk_swapchain;

    WindowImage *images;
} Window;

void vk_window_create(Window *r_window, const char *p_title, size_t p_width, size_t p_height);

void vk_window_free(Window *r_window);

#endif
