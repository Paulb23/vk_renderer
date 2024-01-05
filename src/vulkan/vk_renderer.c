#include "vk_renderer.h"

#include <stdbool.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include "src/math/angles.h"
#include "src/object.h"
#include "src/camera.h"
#include "src/io/memory.h"
#include "src/io/io.h"
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
    CRASH_COND_MSG(vkResetCommandBuffer(*p_command_buffer, 0) != VK_SUCCESS, "%s", "FATAL: Failed to reset command buffer");
    CRASH_COND_MSG(vkBeginCommandBuffer(*p_command_buffer, &(VkCommandBufferBeginInfo){
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = p_flags,
        .pInheritanceInfo = NULL,
    }) != VK_SUCCESS,
    "%s", "FATAL: Failed to start command buffer recording!");
}

static void command_buffer_submit(const Window *p_window, const VkCommandBuffer *p_command_buffer, const VkFence p_fence) {
    CRASH_COND_MSG(vkEndCommandBuffer(*p_command_buffer) != VK_SUCCESS, "%s", "FATAL: Failed to end command buffer!");
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

    CRASH_COND_MSG(vkBindBufferMemory(p_device, *p_buffer, *p_buffer_memory, 0) != VK_SUCCESS, "%s", "FATAL: Failed to bind vKBuffer!");
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
    CRASH_COND_MSG(vkQueueWaitIdle(p_window->vk_queue) != VK_SUCCESS, "%s", "FATAL: Failed to wait for queue!");
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
    CRASH_COND_MSG(vkQueueWaitIdle(p_window->vk_queue) != VK_SUCCESS, "%s", "FATAL: Failed to wait for queue!");
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
    CRASH_COND_MSG(vkQueueWaitIdle(p_window->vk_queue) != VK_SUCCESS, "%s", "FATAL: Failed to wait for queue!");
    command_bufffer_free(p_vk_renderer, p_window, &cmd_buffer);

    // Free memory
    vkDestroyBuffer(p_window->vk_device, staging_buffer, NULL);
    vkFreeMemory(p_window->vk_device, staging_buffer_memory, NULL);
}

// Pipeline

static void pipeline_create_shader_module(VkDevice p_device, const char *p_path, VkShaderModule *r_shader_module) {
    size_t shader_len = { 0 };
    char *shader = read_file(p_path, &shader_len);
    CRASH_NULL_MSG(shader, "FATAL: Failed to load shader from: '%s'", p_path);

    CRASH_COND_MSG(vkCreateShaderModule(p_device,
        &(VkShaderModuleCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = shader_len,
            .pCode = (uint32_t *)shader,
        }, NULL, r_shader_module),
        "%s", "FATAL: Failed to create shader module");
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

    // Create descriptor set layouts
    CRASH_COND_MSG(vkCreateDescriptorSetLayout(p_window->vk_device, &(VkDescriptorSetLayoutCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings = (VkDescriptorSetLayoutBinding[]) {
            {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                .pImmutableSamplers = NULL,
            },
            {
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = NULL,
            },
        },
    },
    NULL, &r_vk_renderer->descriptor_set) != VK_SUCCESS,
    "%s", "FATAL: Failed to create descriptor set layout");

    // Create pool for DescriptorSetLayout
    CRASH_COND_MSG(vkCreateDescriptorPool(p_window->vk_device,
        &(VkDescriptorPoolCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .poolSizeCount = 2,
            .pPoolSizes = (VkDescriptorPoolSize[]) {
                {
                    .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .descriptorCount = 1000,
                },
                {
                    .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = 1000,
                },
            },
            .maxSets = 10000,
        }, NULL, &r_vk_renderer->descriptor_pool) != VK_SUCCESS,
        "%s", "Failed to create descriptor set pool");

    // Create pipeline layout
    CRASH_COND_MSG(vkCreatePipelineLayout(p_window->vk_device, &(VkPipelineLayoutCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &r_vk_renderer->descriptor_set,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = NULL,
        }, NULL, &r_vk_renderer->pipeline_layout) != VK_SUCCESS,
        "%s", "FATAL: Failed tp create pipeline layout");

    // Create renderpass
    CRASH_COND_MSG(vkCreateRenderPass(p_window->vk_device,
        &(VkRenderPassCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = 2,
            .pAttachments = (VkAttachmentDescription[]) {
                {
                    .format = p_window->vk_surface_format.format,
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                },
                {
                    .format = p_window->vk_depth_format,
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                }
            },
            .subpassCount = 1,
            .pSubpasses = (VkSubpassDescription[]) {
                {
                    .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                    .colorAttachmentCount = 1,
                    .pColorAttachments = &(VkAttachmentReference) {
                        .attachment = 0,
                        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    },
                    .pDepthStencilAttachment = &(VkAttachmentReference) {
                        .attachment = 1,
                        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    },
                },
            },
            .dependencyCount = 1,
            .pDependencies = &(VkSubpassDependency) {
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                .srcAccessMask = 0,
                .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            }
    }, NULL, &r_vk_renderer->renderpass),
    "%s", "FATAL: Failed to create render pass");

    // Create FrameBuffers
    CRASH_COND_MSG(vkCreateImage(p_window->vk_device,
        &(VkImageCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .extent.width = p_window->vk_extent2D.width,
            .extent.height = p_window->vk_extent2D.height,
            .extent.depth = 1,
            .mipLevels = 1,
            .arrayLayers = 1,
            .format = p_window->vk_depth_format,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        }, NULL, &r_vk_renderer->depth_texture.image) != VK_SUCCESS,
        "%s", "FATAL: failed to create depth image");

    memory_create_image_buffer(p_window->vk_device, p_window->vk_physical_device, r_vk_renderer->depth_texture.image, &r_vk_renderer->depth_texture.device_memory);

    CRASH_COND_MSG(vkCreateImageView(p_window->vk_device,
        &(VkImageViewCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = r_vk_renderer->depth_texture.image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = p_window->vk_depth_format,
            .subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .subresourceRange.baseMipLevel = 0,
            .subresourceRange.levelCount = 1,
            .subresourceRange.baseArrayLayer = 0,
            .subresourceRange.layerCount = 1,
        }, NULL, &r_vk_renderer->depth_texture.image_view) != VK_SUCCESS,
        "%s", "FATAL: failed to create depth image view");

    r_vk_renderer->vk_frame_buffers = mmalloc(sizeof(VkFramebuffer) * p_window->image_count);
    for (uint32_t i = 0; i < p_window->image_count; i ++) {
        CRASH_COND_MSG(vkCreateFramebuffer(p_window->vk_device,
            &(VkFramebufferCreateInfo){
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass = r_vk_renderer->renderpass,
                .attachmentCount = 2,
                .pAttachments = (VkImageView[]) {
                    p_window->images[i].vk_image_view,
                    r_vk_renderer->depth_texture.image_view,
                },
                .width = p_window->vk_extent2D.width,
                .height = p_window->vk_extent2D.height,
                .layers = 1,
            }, NULL, &r_vk_renderer->vk_frame_buffers[i]),
            "%s", "FATAL: Failed to create frame buffer!");
    }

    // Form into pipeline
    char shader_path[512];
    get_resource_path(shader_path, "shaders/vert_shader.spv");
    pipeline_create_shader_module(p_window->vk_device, shader_path, &r_vk_renderer->vert_shader_module);
    get_resource_path(shader_path, "shaders/frag_shader.spv");
    pipeline_create_shader_module(p_window->vk_device, shader_path, &r_vk_renderer->frag_shader_module);

    CRASH_COND_MSG(vkCreateGraphicsPipelines(p_window->vk_device, VK_NULL_HANDLE, 1, &(VkGraphicsPipelineCreateInfo){
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = (VkPipelineShaderStageCreateInfo[]){
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = r_vk_renderer->vert_shader_module,
                .pName = "main",
            },
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = r_vk_renderer->frag_shader_module,
                .pName = "main",
            },
        },
        .pVertexInputState = &(VkPipelineVertexInputStateCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &(VkVertexInputBindingDescription) {
                .binding = 0,
                .stride = sizeof(Vertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            },
            .vertexAttributeDescriptionCount = 3,
            .pVertexAttributeDescriptions = (VkVertexInputAttributeDescription[]) {
                {
                    .binding = 0,
                    .location = 0,
                    .format = VK_FORMAT_R32G32B32_SFLOAT,
                    .offset = offsetof(Vertex, pos),
                },
                {
                    .binding = 0,
                    .location = 1,
                    .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                    .offset = offsetof(Vertex, color),
                },
                {
                    .binding = 0,
                    .location = 2,
                    .format = VK_FORMAT_R32G32_SFLOAT,
                    .offset = offsetof(Vertex, tex_coord),
                },
            },
        },
        .pInputAssemblyState = &(VkPipelineInputAssemblyStateCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE,
        },
        .pViewportState = &(VkPipelineViewportStateCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1,
        },
        .pRasterizationState = &(VkPipelineRasterizationStateCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .depthClampEnable = VK_FALSE,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_CLOCKWISE,
            .depthBiasEnable = VK_FALSE,
            .depthBiasConstantFactor = 0.0,
            .depthBiasClamp = 0.0,
            .depthBiasSlopeFactor = 0.0,
            .lineWidth = 1.0,
        },
        .pMultisampleState = &(VkPipelineMultisampleStateCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .sampleShadingEnable = VK_FALSE,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .minSampleShading = 1.0,
            .pSampleMask = NULL,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE,
        },
        .pDepthStencilState = &(VkPipelineDepthStencilStateCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = VK_COMPARE_OP_LESS,
            .depthBoundsTestEnable = VK_FALSE,
            .minDepthBounds = 0.0f,
            .maxDepthBounds = 1.0f,
            .stencilTestEnable = VK_FALSE,
            .front = {},
            .back = {},
        },
        .pColorBlendState = &(VkPipelineColorBlendStateCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable = VK_FALSE,
            .logicOp = VK_LOGIC_OP_COPY,
            .attachmentCount = 1,
            .pAttachments = &(VkPipelineColorBlendAttachmentState) {
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
                .blendEnable = VK_TRUE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp = VK_BLEND_OP_ADD,
            },
            .blendConstants[0] = 0.0,
            .blendConstants[1] = 0.0,
            .blendConstants[2] = 0.0,
            .blendConstants[3] = 0.0,
        },
        .pDynamicState = &(VkPipelineDynamicStateCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = 2,
            .pDynamicStates = (VkDynamicState[]) {
                VK_DYNAMIC_STATE_VIEWPORT,
                VK_DYNAMIC_STATE_SCISSOR,
            },
        },
        .layout = r_vk_renderer->pipeline_layout,
        .renderPass = r_vk_renderer->renderpass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1,
    }, NULL, &r_vk_renderer->pipeline) != VK_SUCCESS,
    "%s", "FATAL: Failed to create pipeline!");

    // Create other configuration types
    CRASH_COND_MSG(vkCreateSampler(p_window->vk_device, &(VkSamplerCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = p_window->max_sampler_anisotropy,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .mipLodBias = 0.0f,
        .minLod = 0.0f,
        .maxLod = 0.0f,
    }, NULL, &r_vk_renderer->image_sampler) != VK_SUCCESS,
    "%s", "FATAL: failed to create image sampler!");

    r_vk_renderer->vk_viewport = (VkViewport){
        .x = 0.0f,
        .y = 0.0f,
        .width = p_window->vk_extent2D.width,
        .height = p_window->vk_extent2D.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    r_vk_renderer->vk_scissor = (VkRect2D){
        .extent = p_window->vk_extent2D,
        .offset = {0, 0},
    };
}

void vk_draw_frame(VkRenderer *p_vk_renderer, const Window *p_window, Camera *camera, const Vector *objects) {
    size_t frame = p_vk_renderer->current_frame;

    // Wait for previous frame
    CRASH_COND_MSG(vkWaitForFences(p_window->vk_device, 1, &p_vk_renderer->frame_data[frame].render_fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS, "%s", "FATAL: Failed to wait for frame!");
    CRASH_COND_MSG(vkResetFences(p_window->vk_device, 1, &p_vk_renderer->frame_data[frame].render_fence) != VK_SUCCESS, "%s", "FATAL: Failed to reset frame fence!");

    // Get next image
    uint32_t image_idx;
    CRASH_COND_MSG(vkAcquireNextImageKHR(p_window->vk_device, p_window->vk_swapchain, UINT64_MAX, p_vk_renderer->frame_data[frame].image_available, VK_NULL_HANDLE, &image_idx) != VK_SUCCESS,
        "%s", "FATAL: Failed to get frame image index!");

    const VkCommandBuffer cmd_buffer = p_vk_renderer->frame_data[frame].command_buffer;
    command_buffer_start(&cmd_buffer, 0);

    vkCmdBeginRenderPass(cmd_buffer, &(VkRenderPassBeginInfo) {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = p_vk_renderer->renderpass,
        .framebuffer = p_vk_renderer->vk_frame_buffers[image_idx],
        .renderArea = {
            .offset = {0, 0},
            .extent = p_window->vk_extent2D,
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
    }, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, p_vk_renderer->pipeline);
    vkCmdSetViewport(cmd_buffer, 0, 1, &p_vk_renderer->vk_viewport);
    vkCmdSetScissor(cmd_buffer, 0, 1, &p_vk_renderer->vk_scissor);

    // Create CameraBuffer
    CameraBuffer camera_bufffer = { .proj = { {0},{0},{0},{0} }};

    // Compute camera view
    camera_get_bias(camera, camera_bufffer.view);
    mat4_perspective(camera_bufffer.proj, degtorad(45), p_window->vk_extent2D.width / p_window->vk_extent2D.height, 0.1, 100.0);
    camera_bufffer.proj[1][1] *= -1;

    // TODO: Should take surfaces?
    for (size_t i = 0; i < objects->size; i++) {
        Object object = *(Object *)vector_get(objects, i);

        object_get_bias(&object, camera_bufffer.model);
        SurfaceDescriptorSet surface_descriptor = *(SurfaceDescriptorSet *)vector_get(&object.surface.descriptor_sets, frame);
        memcpy(surface_descriptor.camera_data, &camera_bufffer, sizeof(CameraBuffer));

        vkCmdBindVertexBuffers(cmd_buffer, 0, 1, (VkBuffer[]){object.surface.vertex_buffer}, (VkDeviceSize[]){ 0 });

        vkCmdBindIndexBuffer(cmd_buffer, object.surface.index_buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, p_vk_renderer->pipeline_layout, 0, 1, &surface_descriptor.descriptor_set, 0, NULL);

        vkCmdDrawIndexed(cmd_buffer, object.surface.index_data.size, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(cmd_buffer);
    CRASH_COND_MSG(vkEndCommandBuffer(cmd_buffer) != VK_SUCCESS, "%s", "FATAL: Failed to end command draw buffer!");
    CRASH_COND_MSG(vkQueueSubmit(p_window->vk_queue, 1,
        &(VkSubmitInfo) {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = (VkSemaphore[]) {
                p_vk_renderer->frame_data[frame].image_available,
            },
            .pWaitDstStageMask = (VkPipelineStageFlags[]) {
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
            },
            .commandBufferCount = 1,
            .pCommandBuffers = &cmd_buffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = (VkSemaphore[]) {
                p_vk_renderer->frame_data[frame].render_finished,
            },
        }, p_vk_renderer->frame_data[frame].render_fence) != VK_SUCCESS,
        "%s", "FATAL: Failed to submit queue!");

    CRASH_COND_MSG(vkQueuePresentKHR(p_window->vk_queue,
        &(VkPresentInfoKHR) {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = (VkSemaphore[]) {
                p_vk_renderer->frame_data[p_vk_renderer->current_frame].render_finished,
            },
            .swapchainCount = 1,
            .pSwapchains = (VkSwapchainKHR[]) {
                p_window->vk_swapchain,
            },
            .pImageIndices = &image_idx,
            .pResults = NULL,
        }) != VK_SUCCESS,
        "%s", "FATAL: Failed to present image!");

    p_vk_renderer->current_frame = (p_vk_renderer->current_frame + 1) % p_vk_renderer->frames;
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

void texture_free(const Window *p_window, Texture *p_texture) {
    vkDestroyImageView(p_window->vk_device, p_texture->image_view, NULL);
    vkDestroyImage(p_window->vk_device, p_texture->image, NULL);
    vkFreeMemory(p_window->vk_device, p_texture->device_memory, NULL);
    mfree(p_texture);
}

/// Surface

void surface_descriptor_set_create(const VkRenderer *p_vk_renderer, const Window *p_window, VkImageView *texture, SurfaceDescriptorSet *r_surface_descriptor_set) {
    CRASH_COND_MSG(vkAllocateDescriptorSets(p_window->vk_device, &(VkDescriptorSetAllocateInfo) {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = p_vk_renderer->descriptor_pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &p_vk_renderer->descriptor_set,
        }, &r_surface_descriptor_set->descriptor_set) != VK_SUCCESS,
        "%s", "FATAL: Failed to allocate surface DescriptorSet!");

    memory_create_vkbuffer(p_window->vk_device, p_window->vk_physical_device, sizeof(CameraBuffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &r_surface_descriptor_set->buffer, &r_surface_descriptor_set->memory);

    CRASH_COND_MSG(vkMapMemory(p_window->vk_device, r_surface_descriptor_set->memory, 0, sizeof(CameraBuffer), 0, &r_surface_descriptor_set->camera_data) != VK_SUCCESS, "%s", "FATAL: Failed to map surface DescriptorSet buffer!");

    // Inital update
    vkUpdateDescriptorSets(p_window->vk_device, 2, (VkWriteDescriptorSet[]){
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = r_surface_descriptor_set->descriptor_set,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .pBufferInfo = &(VkDescriptorBufferInfo) {
                .buffer = r_surface_descriptor_set->buffer,
                .offset = 0,
                .range = sizeof(CameraBuffer),
            },
            .pImageInfo = NULL,
            .pTexelBufferView = NULL,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = r_surface_descriptor_set->descriptor_set,
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .pImageInfo = &(VkDescriptorImageInfo) {
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .imageView = *texture,
                .sampler = p_vk_renderer->image_sampler,
            },
            .pBufferInfo = NULL,
            .pTexelBufferView = NULL,
        }
    }, 0, NULL);
}

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

    // Allocate DescriptorSets
    surface->descriptor_sets = (Vector){0, 0, sizeof(SurfaceDescriptorSet), NULL};
    vector_resize(&surface->descriptor_sets, p_vk_renderer->frames);
    for (size_t i = 0; i < p_vk_renderer->frames; i++) {
        surface_descriptor_set_create(p_vk_renderer, p_window, &surface->texture->image_view, vector_get(&surface->descriptor_sets, i));
    }
    return surface;
}

void surface_free(const Window *p_window, Surface *r_surface) {
    vkDestroyBuffer(p_window->vk_device, r_surface->vertex_buffer, NULL);
    vkFreeMemory(p_window->vk_device, r_surface->vertex_memory, NULL);
    vkDestroyBuffer(p_window->vk_device,r_surface->index_buffer, NULL);
    vkFreeMemory(p_window->vk_device, r_surface->index_memory, NULL);

    // TODO: Remove when texture cache exists
    texture_free(p_window, r_surface->texture);
    mfree(r_surface);
}


