#ifndef VK_RENDERER_H_
#define VK_RENDERER_H_

#include "src/data_structures/vector.h"
#include "src/math/vectors.h"
#include "src/math/matrices.h"
#include <SDL2/SDL.h>
#include <vulkan/vulkan.h>
#include <SDL2/SDL_vulkan.h>
#include "vk_window.h"

typedef struct CameraBuffer {
    Mat4 model;
    Mat4 view;
    Mat4 proj;
} CameraBuffer;

typedef struct Texture {
    VkImage image;
    VkImageView image_view;
    VkDeviceMemory device_memory;
} Texture;

/// FrameData

typedef struct FrameData {
    VkSemaphore image_available;
    VkSemaphore render_finished;
    VkFence render_fence;
    VkCommandBuffer command_buffer;
} FrameData;

typedef struct VkRenderer {
    size_t frames;
    size_t current_frame;
    FrameData *frame_data;
    VkCommandPool command_pool;

    // One fixed pipeline for now.
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
    VkRenderPass renderpass;
    VkShaderModule vert_shader_module;
    VkShaderModule frag_shader_module;

    // One descriptor pool with large .maxSets
    VkDescriptorSetLayout descriptor_set;
    VkDescriptorPool descriptor_pool;

    // FrameBuffers
    Texture depth_texture;
    VkFramebuffer *vk_frame_buffers;

    // Only one type for now.
    VkSampler image_sampler;
} VkRenderer;

void vk_renderer_create(VkRenderer *r_vk_renderer, const Window *p_window, size_t p_frame_count);

void vk_renderer_free(VkRenderer *r_vk_renderer, const Window *p_window);

/// Texture

Texture *texture_create(const VkRenderer *p_vk_renderer, const Window *p_window, char *p_path);

void texture_free(const VkRenderer *p_vk_renderer, const Window *p_window, Texture *p_texture);

/// Surface

typedef struct vertex {
    Vect3 pos;
    Vect4 color;
    Vect2 tex_coord;
} Vertex;

// Could use offset rather the buffer per surface
typedef struct SurfaceDescriptorSet {
    VkBuffer buffer;
    VkDeviceMemory memory;
    void *camera_data;
    VkDescriptorSet descriptor_set;
} SurfaceDescriptorSet;

void surface_descriptor_set_create(const VkRenderer *p_vk_renderer, const Window *p_window, VkImageView *texture, SurfaceDescriptorSet *r_surface_descriptor_set);

typedef struct Surface {
    Vector vertex_data;
    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_memory;

    Vector index_data;
    VkBuffer index_buffer;
    VkDeviceMemory index_memory;

    Texture *texture;

    Vector descriptor_sets; // One per frame
} Surface;

Surface *surface_create(const VkRenderer *p_vk_renderer, const Window *p_window, Vector p_vertex, Vector p_index_data, Texture *p_texture);

void surface_free(const VkRenderer *p_vk_renderer, const Window *p_window, Surface *r_surface);

#endif
