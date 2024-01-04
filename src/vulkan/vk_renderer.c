#include "vk_renderer.h"

#include <stdbool.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include "src/io/memory.h"
#include "src/error/error.h"

/// CommandBuffers

static void command_bufffer_create(const VkRenderer *p_vk_renderer, const Window *p_window, VkCommandBuffer *r_command_buffer) {
    CRASH_COND_MSG(vkAllocateCommandBuffers(p_window->vk_device,
    &(VkCommandBufferAllocateInfo) {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = p_vk_renderer->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    },
    r_command_buffer) != VK_SUCCESS,
    "%s", "FATAL: Failed to create command buffer!");
}

static void command_buffer_start(const VkCommandBuffer *p_command_buffer, VkCommandBufferUsageFlags p_flags) {
    vkResetCommandBuffer(*p_command_buffer, 0);
    CRASH_COND_MSG(vkBeginCommandBuffer(*p_command_buffer, &(VkCommandBufferBeginInfo){
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = p_flags,
        .pInheritanceInfo = NULL,
    }) != VK_SUCCESS,
    "%s", "FATAL: Failed to start command buffer recording!");
}

static void command_buffer_submit(const Window *p_window, const VkCommandBuffer *p_command_buffer, const VkFence p_fence) {
    vkEndCommandBuffer(*p_command_buffer);
    CRASH_COND_MSG(vkQueueSubmit(p_window->vk_queue, 1,
        &(VkSubmitInfo){
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = p_command_buffer,
        }, p_fence) != VK_SUCCESS,
    "%s", "FATAL: Failed to submit command buffer!");
}

static void command_bufffer_free(const VkRenderer *p_vk_renderer, const Window *p_window, VkCommandBuffer *r_command_buffer) {
    vkFreeCommandBuffers(p_window->vk_device, p_vk_renderer->command_pool, 1, r_command_buffer);
}

/// Memory
uint32_t memory_get_requrement_idx(VkMemoryRequirements p_vk_memory_requirements, VkPhysicalDeviceMemoryProperties p_vk_memory_properties, uint32_t p_properties) {
    for (uint32_t i = 0; i < p_vk_memory_properties.memoryTypeCount; i++) {
        if ((p_vk_memory_requirements.memoryTypeBits & (1 << i)) && (p_vk_memory_properties.memoryTypes[i].propertyFlags & p_properties) == p_properties) {
            return i;
        }
    }
    CRASH_COND_MSG(true, "%s", "FATAL: Failed to find valid memory type for buffer!");
}

void memory_create_vkbuffer(VkDevice p_device, VkPhysicalDevice p_physical_device, VkDeviceSize p_size, VkBufferUsageFlags p_usage, VkMemoryPropertyFlags p_properties, VkBuffer *p_buffer, VkDeviceMemory *p_buffer_memory) {
    CRASH_COND_MSG(vkCreateBuffer(p_device, &(VkBufferCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = p_size,
        .usage = p_usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    }, NULL, p_buffer) != VK_SUCCESS,
    "%s", "FATAL: Failed to create memory buffer!");

    VkMemoryRequirements vk_memory_requrements;
    vkGetBufferMemoryRequirements(p_device, *p_buffer, &vk_memory_requrements);

    VkPhysicalDeviceMemoryProperties vk_memory_properties;
    vkGetPhysicalDeviceMemoryProperties(p_physical_device, &vk_memory_properties);

    // Check max supported allocations, and use offset instead
    CRASH_COND_MSG(vkAllocateMemory(p_device,
        &(VkMemoryAllocateInfo) {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = vk_memory_requrements.size,
            .memoryTypeIndex = memory_get_requrement_idx(vk_memory_requrements, vk_memory_properties, p_properties),
        }, NULL, p_buffer_memory) != VK_SUCCESS,
        "%s", "FATAL: Failed to allocate memory for buffer!");

    vkBindBufferMemory(p_device, *p_buffer, *p_buffer_memory, 0);
}

static void memory_create_image_buffer(VkDevice p_device, VkPhysicalDevice p_physical_device, VkImage p_image, VkDeviceMemory *r_buffer) {
    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(p_device, p_image, &memory_requirements);

    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(p_physical_device, &memory_properties);

    CRASH_COND_MSG(vkAllocateMemory(p_device,
        &(VkMemoryAllocateInfo) {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memory_requirements.size,
            .memoryTypeIndex = memory_get_requrement_idx(memory_requirements, memory_properties, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
        }, NULL, r_buffer) != VK_SUCCESS,
        "%s", "FATAL: Failed to allocate memory for image buffer!");
    CRASH_COND_MSG(vkBindImageMemory(p_device, p_image, *r_buffer, 0) != VK_SUCCESS, "%s", "FATAL: Failed to bind image buffer!");
}

static void memory_transition_image(const VkRenderer *p_vk_renderer, const Window *p_window, VkImage p_image, VkImageLayout p_old_layout, VkImageLayout p_new_layout) {
    VkCommandBuffer cmd_buffer;
    command_bufffer_create(p_vk_renderer, p_window, &cmd_buffer);
    command_buffer_start(&cmd_buffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VkPipelineStageFlags source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkAccessFlags source_flags = 0;

    VkPipelineStageFlags destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkAccessFlags destination_flags = VK_ACCESS_TRANSFER_WRITE_BIT;

    if (p_old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && p_new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        source_flags = VK_ACCESS_TRANSFER_WRITE_BIT;

        destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        destination_flags = VK_ACCESS_SHADER_READ_BIT;
    }

    vkCmdPipelineBarrier(cmd_buffer, source_stage, destination_stage, 0, 0, NULL, 0, NULL, 1, &(VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = p_old_layout,
        .newLayout = p_new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = p_image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .srcAccessMask = source_flags,
        .dstAccessMask = destination_flags,
    });

    command_buffer_submit(p_window, &cmd_buffer, VK_NULL_HANDLE);
    vkQueueWaitIdle(p_window->vk_queue);
    command_bufffer_free(p_vk_renderer, p_window, &cmd_buffer);
}

static void memory_upload_image(const VkRenderer *p_vk_renderer, const Window *p_window, VkImage p_image, void *p_data, int p_texture_width, int p_texture_height, VkDeviceMemory *r_buffer) {
    // Create and load into stating buffer
    VkDeviceSize data_size = p_texture_width * p_texture_height * 4;
    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;
    memory_create_vkbuffer(p_window->vk_device, p_window->vk_physical_device, data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging_buffer, &staging_buffer_memory);

    void *image_data;
    CRASH_COND_MSG(vkMapMemory(p_window->vk_device, staging_buffer_memory, 0, data_size, 0, &image_data) != VK_SUCCESS, "%s", "FATAL: Failed to map device memory!");
    memcpy(image_data, p_data, data_size);
    vkUnmapMemory(p_window->vk_device, staging_buffer_memory);

    // Load into device
    memory_create_image_buffer(p_window->vk_device, p_window->vk_physical_device, p_image, r_buffer);

    // Update format
    memory_transition_image(p_vk_renderer, p_window, p_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Reupload new format
    VkCommandBuffer cmd_buffer;
    command_bufffer_create(p_vk_renderer, p_window, &cmd_buffer);
    command_buffer_start(&cmd_buffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    vkCmdCopyBufferToImage(cmd_buffer, staging_buffer, p_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &(VkBufferImageCopy) {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,

        .imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .imageSubresource.mipLevel = 0,
        .imageSubresource.baseArrayLayer = 0,
        .imageSubresource.layerCount = 1,

        .imageOffset = {0, 0, 0},
        .imageExtent = {
            (uint32_t)p_texture_width,
            (uint32_t)p_texture_height,
            1
        },
    });
    command_buffer_submit(p_window, &cmd_buffer, VK_NULL_HANDLE);
    vkQueueWaitIdle(p_window->vk_queue);
    command_bufffer_free(p_vk_renderer, p_window, &cmd_buffer);

    // Update format
    memory_transition_image(p_vk_renderer, p_window, p_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Free memory
    vkDestroyBuffer(p_window->vk_device, staging_buffer, NULL);
    vkFreeMemory(p_window->vk_device, staging_buffer_memory, NULL);
}

static void memory_upload_data(const VkRenderer *p_vk_renderer, const Window *p_window, void *p_data, size_t p_data_size, VkBuffer *r_vk_buffer, VkDeviceMemory *r_device_memory, VkBufferUsageFlags p_useage_flags) {
    // Create and load into stating buffer
    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;
    memory_create_vkbuffer(p_window->vk_device, p_window->vk_physical_device, p_data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging_buffer, &staging_buffer_memory);

    void *data;
    CRASH_COND_MSG(vkMapMemory(p_window->vk_device, staging_buffer_memory, 0, p_data_size, 0, &data) != VK_SUCCESS, "%s", "FATAL: Failed to map device memory!");
    memcpy(data, p_data, p_data_size);
    vkUnmapMemory(p_window->vk_device, staging_buffer_memory);

    // Create device buffer
    memory_create_vkbuffer(p_window->vk_device, p_window->vk_physical_device, p_data_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | p_useage_flags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, r_vk_buffer, r_device_memory);

    // Upload data
    VkCommandBuffer cmd_buffer;
    command_bufffer_create(p_vk_renderer, p_window, &cmd_buffer);
    command_buffer_start(&cmd_buffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    vkCmdCopyBuffer(cmd_buffer, staging_buffer, *r_vk_buffer, 1, &(VkBufferCopy){
        .srcOffset = 0,
        .dstOffset = 0,
        .size = p_data_size,
    });

    command_buffer_submit(p_window, &cmd_buffer, VK_NULL_HANDLE);
    vkQueueWaitIdle(p_window->vk_queue);
    command_bufffer_free(p_vk_renderer, p_window, &cmd_buffer);

    // Free memory
    vkDestroyBuffer(p_window->vk_device, staging_buffer, NULL);
    vkFreeMemory(p_window->vk_device, staging_buffer_memory, NULL);
}

// Interface

void vk_renderer_create(VkRenderer *r_vk_renderer, const Window *p_window, size_t p_frame_count) {
    CRASH_COND_MSG(p_frame_count > p_window->image_count, "%s", "FATAL: Not enough images in swapchain!");

    // Create command pool
    CRASH_COND_MSG(vkCreateCommandPool(p_window->vk_device,
        &(VkCommandPoolCreateInfo){
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = p_window->vk_queue_index,
        },
        NULL, &r_vk_renderer->command_pool) != VK_SUCCESS,
        "%s", "FATAL: Failed to create command pool");

    // ALlocate per FrameData
    r_vk_renderer->current_frame = 0;
    r_vk_renderer->frames = p_frame_count;
    r_vk_renderer->frame_data = mmalloc(sizeof(FrameData) * p_frame_count);
    for (size_t i = 0; i < p_frame_count; i++) {

        CRASH_COND_MSG(vkCreateSemaphore(p_window->vk_device,
            &(VkSemaphoreCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            },
            NULL, &r_vk_renderer->frame_data[i].image_available) != VK_SUCCESS,
            "%s", "FATAL: Failed to create image semaphore!");

        CRASH_COND_MSG(vkCreateSemaphore(p_window->vk_device,
            &(VkSemaphoreCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            },
            NULL, &r_vk_renderer->frame_data[i].render_finished) != VK_SUCCESS,
            "%s", "FATAL: Failed to create render semaphore!");

        CRASH_COND_MSG(vkCreateFence(p_window->vk_device,
            &(VkFenceCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .flags = VK_FENCE_CREATE_SIGNALED_BIT,
            },
            NULL, &r_vk_renderer->frame_data[i].render_fence) != VK_SUCCESS,
            "%s", "FATAL: Failed to create frame fence!");

        command_bufffer_create(r_vk_renderer, p_window, &r_vk_renderer->frame_data[i].command_buffer);
    };
}

void vk_renderer_free(VkRenderer *r_vk_renderer, const Window *p_window) {
    for (size_t i = 0; i < r_vk_renderer->frames; i++) {
        vkDestroySemaphore(p_window->vk_device, r_vk_renderer->frame_data[i].image_available, NULL);
        vkDestroySemaphore(p_window->vk_device, r_vk_renderer->frame_data[i].render_finished, NULL);
        vkDestroyFence(p_window->vk_device, r_vk_renderer->frame_data[i].render_fence, NULL);
    }
    // Will automaticaly free any CommandBuffers in the pool
    vkDestroyCommandPool(p_window->vk_device, r_vk_renderer->command_pool, NULL);

    mfree(r_vk_renderer->frame_data);
}

/// Texture

Texture *texture_create(const VkRenderer *p_vk_renderer, const Window *p_window, char *p_path) {
    // TODO: Add cache

    int texture_width = 0;
    int texture_height = 0;
    int texture_channels = 0;

    stbi_uc* pixels = stbi_load(p_path, &texture_width, &texture_height, &texture_channels, STBI_rgb_alpha);
    CRASH_NULL_MSG(pixels, "FATAL: Failed to load texture '%s'!", p_path);

    // Create image
    VkImage vk_image;
    CRASH_COND_MSG(vkCreateImage(p_window->vk_device, &(VkImageCreateInfo){
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .extent.width = texture_width,
        .extent.height = texture_height,
        .extent.depth = 1,
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .flags = 0,
    }, NULL, &vk_image) != VK_SUCCESS,
    "%s", "FATAL: Failed to create image!");

    // Upload to the GPU
    VkDeviceMemory buffer;
    memory_upload_image(p_vk_renderer, p_window, vk_image, pixels, texture_width, texture_height, &buffer);

    // Crete image view
    VkImageView image_view;
    CRASH_COND_MSG(vkCreateImageView(p_window->vk_device, &(VkImageViewCreateInfo){
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = vk_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .subresourceRange.baseMipLevel = 0,
        .subresourceRange.levelCount = 1,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount = 1,
    }, NULL, &image_view) != VK_SUCCESS,
    "%s", "FATAL: Failed to create image image view!");

    // Free memory
    stbi_image_free(pixels);

    // Create and return
    Texture *texture = mmalloc(sizeof(texture));
    texture->image = vk_image;
    texture->image_view = image_view;
    texture->device_memory = buffer;
    return texture;
}

void texture_free(const VkRenderer *p_vk_renderer, const Window *p_window, Texture *p_texture) {
    vkDestroyImageView(p_window->vk_device, p_texture->image_view, NULL);
    vkDestroyImage(p_window->vk_device, p_texture->image, NULL);
    vkFreeMemory(p_window->vk_device, p_texture->device_memory, NULL);
    mfree(p_texture);
}

/// Surface

Surface *surface_create(const VkRenderer *p_vk_renderer, const Window *p_window, Vector p_vertex, Vector p_index_data, Texture *p_texture) {
    // TODO: Cache to re-use same memory
    Surface *surface = mmalloc(sizeof(Surface));

    surface->vertex_data = (Vector){0, 0, sizeof(Vertex), NULL};
    vector_copy(&p_vertex, &surface->vertex_data);
    memory_upload_data(p_vk_renderer, p_window, surface->vertex_data.data, surface->vertex_data.data_size * surface->vertex_data.size, &surface->vertex_buffer, &surface->vertex_memory, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    surface->index_data = (Vector){0, 0, sizeof(uint32_t), NULL};
    vector_copy(&p_index_data, &surface->index_data);
    memory_upload_data(p_vk_renderer, p_window, surface->index_data.data, surface->index_data.data_size * surface->index_data.size, &surface->index_buffer, &surface->index_memory, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    surface->texture = p_texture;
    return surface;
}

void surface_free(const VkRenderer *p_vk_renderer, const Window *p_window, Surface *r_surface) {
    vkDestroyBuffer(p_window->vk_device, r_surface->vertex_buffer, NULL);
    vkFreeMemory(p_window->vk_device, r_surface->vertex_memory, NULL);
    vkDestroyBuffer(p_window->vk_device,r_surface->index_buffer, NULL);
    vkFreeMemory(p_window->vk_device, r_surface->index_memory, NULL);

    // TODO: Remove when texture cache exists
    texture_free(p_vk_renderer, p_window, r_surface->texture);
    mfree(r_surface);
}


