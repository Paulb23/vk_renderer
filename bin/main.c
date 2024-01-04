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

void create_vkbuffer(VkDevice p_device, VkPhysicalDevice p_physical_device, VkDeviceSize p_size, VkBufferUsageFlags p_usage, VkMemoryPropertyFlags p_properties, VkBuffer *p_buffer, VkDeviceMemory *p_buffer_memory) {
    VkBufferCreateInfo buffer_crreate_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = p_size,
        .usage = p_usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VkResult error = vkCreateBuffer(p_device, &buffer_crreate_info, NULL, p_buffer);
    if (error != VK_SUCCESS) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to allocate vertex buffer!", NULL);
        return;
    }

    VkMemoryRequirements vk_memory_requrements;
    vkGetBufferMemoryRequirements(p_device, *p_buffer, &vk_memory_requrements);

    VkPhysicalDeviceMemoryProperties vk_memory_properties;
    vkGetPhysicalDeviceMemoryProperties(p_physical_device, &vk_memory_properties);

    uint32_t memory_type_idx = 0;
    bool found = false;
    for (uint32_t i = 0; i < vk_memory_properties.memoryTypeCount; i++) {
        if ((vk_memory_requrements.memoryTypeBits & (1 << i)) && (vk_memory_properties.memoryTypes[i].propertyFlags & p_properties) == p_properties) {
            found = true;
            memory_type_idx = i;
            break;
        }
    }
    if (!found) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to find valid memory type!", NULL);
        return;
    }

    VkMemoryAllocateInfo memory_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = vk_memory_requrements.size,
        .memoryTypeIndex = memory_type_idx,
    };

    // Check max supported allocations, and use offset instead
    error = vkAllocateMemory(p_device, &memory_allocate_info, NULL, p_buffer_memory);
    if (error != VK_SUCCESS) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to allocate vertex buffer!", NULL);
        return;
    }
    vkBindBufferMemory(p_device, *p_buffer, *p_buffer_memory, 0);
}

typedef struct CameraBuffer {
    Mat4 model;
    Mat4 view;
    Mat4 proj;
} CameraBuffer;

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

    // Load and Create shaders
    char shader_path[512];
    get_resource_path(shader_path, "shaders/vert_shader.spv");
    size_t vert_shader_len = {};
    char *vert_shader = read_file(shader_path, &vert_shader_len);
    if (!vert_shader) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to load vert shader!", NULL);
        goto cleanup_imageviews;
    }

    VkShaderModuleCreateInfo vert_shader_create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = vert_shader_len,
        .pCode = (uint32_t *)vert_shader,
    };

    VkShaderModule vert_shader_module;
    VkResult error = vkCreateShaderModule(window.vk_device, &vert_shader_create_info, NULL, &vert_shader_module);
    if (error != VK_SUCCESS) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to create vert shader!", NULL);
        goto cleanup_vert;
    }

    get_resource_path(shader_path, "shaders/frag_shader.spv");
    size_t frag_shader_len = {};
    char *frag_shader = read_file(shader_path, &frag_shader_len);
    if (!frag_shader) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to load frag shader!", NULL);
        goto cleanup_vert;
    }

    VkShaderModuleCreateInfo frag_shader_create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = frag_shader_len,
        .pCode = (uint32_t *)frag_shader,
    };

    VkShaderModule frag_shader_module;
    error = vkCreateShaderModule(window.vk_device, &frag_shader_create_info, NULL, &frag_shader_module);
    if (error != VK_SUCCESS) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to create frag shader!", NULL);
        goto cleanup_frag;
    }

    // Create shaderPipeline
    VkPipelineShaderStageCreateInfo vert_shader_stage_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vert_shader_module,
        .pName = "main",
    };

    VkPipelineShaderStageCreateInfo frag_shader_stage_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = frag_shader_module,
        .pName = "main",
    };

    // Create Fixed functions Pipelines

    // Define vertex data
    VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info = {
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
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info ={
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    VkPipelineRasterizationStateCreateInfo rasterizer_state_create_info = {
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
    };

    VkPipelineMultisampleStateCreateInfo multisample_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .minSampleShading = 1.0,
        .pSampleMask = NULL,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE,
    };

    VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {
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
    };

    // Create Viewport
    VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = (VkDynamicState[]) {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        },
    };

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

    VkPipelineViewportStateCreateInfo viewport_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    // Create Descriptor Set
    VkDescriptorSetLayoutCreateInfo camera_descriptor_set_create_info = {
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
    };

    VkDescriptorSetLayout camera_descriptor_set_layout = {};
    error = vkCreateDescriptorSetLayout(window.vk_device, &camera_descriptor_set_create_info, NULL, &camera_descriptor_set_layout);
    if (error != VK_SUCCESS) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to create camera descriptor set!", NULL);
        exit(0);
    }

    VkBuffer camera_data_buffer = {};
    VkDeviceMemory camera_data_buffer_memory = {};
    create_vkbuffer(window.vk_device, window.vk_physical_device, sizeof(CameraBuffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &camera_data_buffer, &camera_data_buffer_memory);

    void *camera_data_ptr;
    vkMapMemory(window.vk_device, camera_data_buffer_memory, 0, sizeof(CameraBuffer), 0, &camera_data_ptr);

    // Create Descriptor pool
    VkDescriptorPoolCreateInfo descriptor_pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = 2,
        .pPoolSizes = (VkDescriptorPoolSize[]) {
            {
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
            },
            {
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
            },
        },
        .maxSets = 1,
    };

    VkDescriptorPool vk_descriptor_pool = {};
    error = vkCreateDescriptorPool(window.vk_device, &descriptor_pool_create_info, NULL, &vk_descriptor_pool);
    if (error != VK_SUCCESS) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to create descriptor pool!", NULL);
        exit(0);
    }

    // Allocate DescriptorSets
    VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = vk_descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &camera_descriptor_set_layout,
    };

    VkDescriptorSet vk_descriptor_sets;
    error = vkAllocateDescriptorSets(window.vk_device, &descriptor_set_allocate_info, &vk_descriptor_sets);

    // Create the Pipeline layout
    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &camera_descriptor_set_layout,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = NULL,
    };

    VkPipelineLayout vk_pipieline_layout = {};
    error = vkCreatePipelineLayout(window.vk_device, &pipeline_layout_create_info, NULL, &vk_pipieline_layout);
    if (error != VK_SUCCESS) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to create pipeline layput!", NULL);
        goto cleanup_pipeline;
    }

    // Create render passes
    VkRenderPassCreateInfo render_pass_create_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 2,
        .pAttachments = (VkAttachmentDescription[]) {
            {
                .format = window.vk_surface_format.format,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            },
            {
                .format = window.vk_depth_format,
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
    };

    VkRenderPass vk_render_pass = {};
    error = vkCreateRenderPass(window.vk_device, &render_pass_create_info, NULL, &vk_render_pass);
    if (error != VK_SUCCESS) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to create render pass!", NULL);
        goto cleanup_renderpass;
    }

    // Create depth buffer;
    VkImageCreateInfo depth_image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .extent.width = window.vk_extent2D.width,
        .extent.height = window.vk_extent2D.height,
        .extent.depth = 1,
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = window.vk_depth_format,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VkImage depth_image;
    error = vkCreateImage(window.vk_device, &depth_image_info, NULL, &depth_image);
    if (error != VK_SUCCESS) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to create depth image!", NULL);
        exit(0);
    }

    VkMemoryRequirements depth_mem_req;;
    vkGetImageMemoryRequirements(window.vk_device, depth_image, &depth_mem_req);

    VkPhysicalDeviceMemoryProperties depth_vk_memory_properties;
    vkGetPhysicalDeviceMemoryProperties(window.vk_physical_device, &depth_vk_memory_properties);

    uint32_t depth_memory_type_idx = 0;
    uint32_t depth_prop = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    bool found = false;
    for (uint32_t i = 0; i < depth_vk_memory_properties.memoryTypeCount; i++) {
        if ((depth_mem_req.memoryTypeBits & (1 << i)) && (depth_vk_memory_properties.memoryTypes[i].propertyFlags & depth_prop) == depth_prop) {
            found = true;
            depth_memory_type_idx = i;
            break;
        }
    }
    if (!found) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to find valid depth memory type!", NULL);
        exit(0);
    }

    VkMemoryAllocateInfo depth_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = depth_mem_req.size,
        .memoryTypeIndex = depth_memory_type_idx,
    };

    VkDeviceMemory depth_image_memory;
    if (vkAllocateMemory(window.vk_device, &depth_alloc_info, NULL, &depth_image_memory) != VK_SUCCESS) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to allocate memory for depth image!", NULL);
        exit(0);
    }
    vkBindImageMemory(window.vk_device, depth_image, depth_image_memory, 0);

        VkImageViewCreateInfo depth_image_view_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = depth_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = window.vk_depth_format,
        .subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
        .subresourceRange.baseMipLevel = 0,
        .subresourceRange.levelCount = 1,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount = 1,
    };

    VkImageView depth_image_view;
    error = vkCreateImageView(window.vk_device, &depth_image_view_create_info, NULL, &depth_image_view);
    if (error != VK_SUCCESS) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to create depth image view!", NULL);
        exit(0);
    }

    // Create Graphics pipeline
    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = (VkPipelineShaderStageCreateInfo[]){
            vert_shader_stage_info,
            frag_shader_stage_info,
        },
        .pVertexInputState = &vertex_input_state_create_info,
        .pInputAssemblyState = &input_assembly_state_create_info,
        .pViewportState = &viewport_state_create_info,
        .pRasterizationState = &rasterizer_state_create_info,
        .pMultisampleState = &multisample_state_create_info,
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
        .pColorBlendState = &color_blend_state_create_info,
        .pDynamicState = &dynamic_state_create_info,
        .layout = vk_pipieline_layout,
        .renderPass = vk_render_pass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1,
    };

    VkPipeline vk_graphics_pipeline = {};
    error = vkCreateGraphicsPipelines(window.vk_device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, NULL, &vk_graphics_pipeline);
    if (error != VK_SUCCESS) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to create graphics pipeline!", NULL);
        goto vk_graphics_pipeline;
    }

    // Create FromeBuffers
    VkFramebuffer *vk_frame_buffers = mmalloc(sizeof(VkFramebuffer) * window.image_count);

    for (uint32_t i = 0; i < window.image_count; i ++) {
        VkFramebufferCreateInfo frame_buffer_create_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = vk_render_pass,
            .attachmentCount = 2,
            .pAttachments = (VkImageView[]) {
                window.images[i].vk_image_view,
                depth_image_view,
            },
            .width = window.vk_extent2D.width,
            .height = window.vk_extent2D.height,
            .layers = 1,
        };

        error = vkCreateFramebuffer(window.vk_device, &frame_buffer_create_info, NULL, &vk_frame_buffers[i]);
        if (error != VK_SUCCESS) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to create frame buffers!", NULL);
            goto vk_image_buffers;
        }
    }

    // Create command pool
    VkCommandPoolCreateInfo command_pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = window.vk_queue_index,
    };

    VkCommandPool vk_command_pool = {};
    error = vkCreateCommandPool(window.vk_device, &command_pool_create_info, NULL, &vk_command_pool);
    if (error != VK_SUCCESS) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to create command pool!", NULL);
        goto vk_image_buffers;
    }

    // Create command Buffer
    VkCommandBufferAllocateInfo command_buffer_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = vk_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkCommandBuffer vk_command_buffer = {};
    error = vkAllocateCommandBuffers(window.vk_device, &command_buffer_create_info, &vk_command_buffer);
    if (error != VK_SUCCESS) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to create command buffer!", NULL);
        goto vk_command_pool;
    }

    // Create Semaphores
    VkSemaphoreCreateInfo semaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VkSemaphore vk_image_available_semaphore;
    VkSemaphore vk_render_finished_semaphore;
    vkCreateSemaphore(window.vk_device, &semaphoreInfo, NULL, &vk_image_available_semaphore);
    vkCreateSemaphore(window.vk_device, &semaphoreInfo, NULL, &vk_render_finished_semaphore);

    // Create current frame Fence
    VkFenceCreateInfo fence_create_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    VkFence current_frame_fence;
    error = vkCreateFence(window.vk_device, &fence_create_info, NULL, &current_frame_fence);
    if (error != VK_SUCCESS) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to create current frame fence!", NULL);
        goto vk_command_buffer;
    }

    // Create image sampler
    VkSamplerCreateInfo image_sampler_create_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = window.max_sampler_anisotropy,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .mipLodBias = 0.0f,
        .minLod = 0.0f,
        .maxLod = 0.0f,
    };

    VkSampler image_sampler;
    error = vkCreateSampler(window.vk_device, &image_sampler_create_info, NULL, &image_sampler);
    if (error != VK_SUCCESS) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to create image samapler!", NULL);
        exit(0);
    }

    // Set up writes
    VkWriteDescriptorSet write_descriptor_set = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = vk_descriptor_sets,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .pBufferInfo = &(VkDescriptorBufferInfo) {
            .buffer = camera_data_buffer,
            .offset = 0,
            .range = sizeof(CameraBuffer),
        },
        .pImageInfo = NULL,
        .pTexelBufferView = NULL,
    };

    VkWriteDescriptorSet write_descriptor_set_image = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = vk_descriptor_sets,
        .dstBinding = 1,
        .dstArrayElement = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .pImageInfo = &(VkDescriptorImageInfo) {
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .imageView = surface->texture->image_view,
            .sampler = image_sampler,
        },
        .pBufferInfo = NULL,
        .pTexelBufferView = NULL,
    };
    vkUpdateDescriptorSets(window.vk_device, 2, (VkWriteDescriptorSet[]){write_descriptor_set, write_descriptor_set_image}, 0, NULL);

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
        vkWaitForFences(window.vk_device, 1, &current_frame_fence, VK_TRUE, UINT64_MAX);
        vkResetFences(window.vk_device, 1, &current_frame_fence);

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
        mat4_rotate(camera_bufffer.model,  degtorad(90), (Vect3){-1, 0, 0});

        camera_get_bias(&camera, camera_bufffer.view);

        mat4_perspective(camera_bufffer.proj, degtorad(45), (float)window.vk_extent2D.width / (float)window.vk_extent2D.height, (float)0.1, (float)10.0);
        camera_bufffer.proj[1][1] *= -1;

        memcpy(camera_data_ptr, &camera_bufffer, sizeof(CameraBuffer));

        uint32_t imageIndex;
        vkAcquireNextImageKHR(window.vk_device, window.vk_swapchain, UINT64_MAX, vk_image_available_semaphore, VK_NULL_HANDLE, &imageIndex);

        vkResetCommandBuffer(vk_command_buffer, 0);

        // Record commands
        VkCommandBufferBeginInfo command_buffer_begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = 0,
            .pInheritanceInfo = NULL,
        };

        error = vkBeginCommandBuffer(vk_command_buffer, &command_buffer_begin_info);
        if (error != VK_SUCCESS) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to start command buffer recording!", NULL);
            running = false;
        }

        VkRenderPassBeginInfo render_pass_info = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = vk_render_pass,
            .framebuffer = vk_frame_buffers[imageIndex],
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
        vkCmdBeginRenderPass(vk_command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(vk_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_graphics_pipeline);

        vkCmdSetViewport(vk_command_buffer, 0, 1, &vk_viewport);
        vkCmdSetScissor(vk_command_buffer, 0, 1, &vk_scissor);

        vkCmdBindPipeline(vk_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_graphics_pipeline);

        VkBuffer vertexBuffers[] = {surface->vertex_buffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(vk_command_buffer, 0, 1, vertexBuffers, offsets);

        vkCmdBindIndexBuffer(vk_command_buffer, surface->index_buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(vk_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipieline_layout, 0, 1, &vk_descriptor_sets, 0, NULL);

        vkCmdDrawIndexed(vk_command_buffer, surface->index_data.size, 1, 0, 0, 0);

        vkCmdEndRenderPass(vk_command_buffer);
        error = vkEndCommandBuffer(vk_command_buffer);
        if (error != VK_SUCCESS) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to end command buffer recording!", NULL);
            running = false;
        }

        VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = (VkSemaphore[]) {
                vk_image_available_semaphore,
            },
            .pWaitDstStageMask = (VkPipelineStageFlags[]) {
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
            },
            .commandBufferCount = 1,
            .pCommandBuffers = &vk_command_buffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = (VkSemaphore[]) {
                vk_render_finished_semaphore,
            },
        };

        error = vkQueueSubmit(window.vk_queue, 1, &submit_info, current_frame_fence);
        if (error != VK_SUCCESS) {
            running = false;
        }

        VkPresentInfoKHR present_info = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = (VkSemaphore[]) {
                vk_render_finished_semaphore,
            },
            .swapchainCount = 1,
            .pSwapchains = (VkSwapchainKHR[]) {
                window.vk_swapchain,
            },
            .pImageIndices = &imageIndex,
            .pResults = NULL,
        };

        vkQueuePresentKHR(window.vk_queue, &present_info);


        if (SDL_GetTicks() - timer > 1000) {
            timer += 1000;
            uptime++;
            frames = fps;
            fps = 0;
            tick = 0;
        }
    }

    //vkDestroyDescriptorSetLayout(vk_device, camera_descriptor_set_create_info, NULL);
    //vkDestroyBuffer(vk_device, vk_vertex_buffer, NULL);
    //vkFreeMemory(vk_device, vertext_buffer_memory, NULL);

    texture_free(&renderer, &window, texture);
//vk_fence:
    vkDestroyFence(window.vk_device, current_frame_fence, NULL);
vk_command_buffer:
    vkFreeCommandBuffers(window.vk_device, vk_command_pool, 1, &vk_command_buffer);
vk_command_pool:
    vkDestroyCommandPool(window.vk_device, vk_command_pool, NULL);
vk_image_buffers:
    for (uint32_t i = 0; i < window.image_count; i ++) {
        //mfree(vk_frame_buffers[i]);
    }
    mfree(vk_frame_buffers);
    vkDestroyPipeline(window.vk_device, vk_graphics_pipeline, NULL);
vk_graphics_pipeline:
    vkDestroyRenderPass(window.vk_device, vk_render_pass, NULL);
cleanup_renderpass:
    vkDestroyPipelineLayout(window.vk_device, vk_pipieline_layout, NULL);
cleanup_pipeline:
    vkDestroyShaderModule(window.vk_device, frag_shader_module, NULL);
cleanup_frag:
    mfree(frag_shader);
    vkDestroyShaderModule(window.vk_device, vert_shader_module, NULL);
cleanup_vert:
    mfree(vert_shader);
cleanup_imageviews:
    //vkDestroySwapchainKHR(vk_device, vk_swapchain, NULL);
    vk_window_free(&window);
    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();

    return 0;
}
