#include <stdbool.h>
#include <math.h>
#include <SDL2/SDL.h>
#include <vulkan/vulkan.h>
#include <SDL2/SDL_vulkan.h>

static int32_t max_ticks = 60;
static double ns = 0;

static int32_t uptime = 0;
static int32_t frames = 0;

static uint32_t window_width = 800;
static uint32_t window_height = 600;

char *read_file(const char *p_path, size_t *r_file_size) {
    FILE *file = fopen(p_path, "rb");
    if (!file) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    size_t file_size = (size_t)ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = malloc(file_size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }
    fread(buffer, file_size, 1, file);
    fclose(file);

    *r_file_size = file_size;

    /*printf("%s\n", p_path);

    for (size_t i = 0; i < file_size; i++) {
        printf("0x%x ", buffer[i]);
        if (i != 0 && (i % 5) == 0) {
            printf("\n");
        }
    }*/
    return buffer;
}

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

typedef struct vect2 {
    float x;
    float y;
} Vect2;

typedef struct vect3 {
    float x;
    float y;
    float z;
} Vect3;

typedef struct vect4 {
    float r;
    float g;
    float b;
    float a;
} Vect4;

typedef float mat4[4][4];

typedef struct vertex {
    Vect2 pos;
    Vect4 color;
} Vertex;

typedef struct CameraBuffer {
    mat4 model;
    mat4 view;
    mat4 proj;
} CameraBuffer;

Vect3 vect3_normalise(Vect3 a) {
    float v = (float)sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
    return (Vect3) {
        a.x / v,
        a.y / v,
        a.z / v
    };
}

// Need to negate?
float vect3_dot(Vect3 a, Vect3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vect3 vect3_cross(Vect3 a, Vect3 b) {
    return (Vect3) {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

Vect3 vect3_sub(Vect3 a, Vect3 b) {
    return (Vect3) {
        a.x - b.x,
        a.y - b.y,
        a.z - b.z
    };
}

void mat4_rotate(mat4 mat, float rads, Vect3 a) {
    float c = (float)cos(rads);
    float s = (float)sin(rads);

    Vect3 axis = vect3_normalise(a);
    Vect3 temp = {(1 - c) * axis.x, (1 - c) * axis.y, (1 - c) * axis.z };

    mat4 rot_mtx = {
        {c + temp.x * axis.x, temp.x * axis.y + s * axis.z, temp.x * axis.z - s * axis.y, 0},
        {temp.y * axis.x - s * axis.z, c + temp.y * axis.y, temp.y * axis.z + s * axis.x, 0},
        {temp.z * axis.x + s * axis.y, temp.z * axis.y - s * axis.x, c + temp.z * axis.z, 0},
        {0, 0, 0, 1},
    };

    mat4 ret_mtx;
    for(int i = 0; i < 4; i++) {
        for(int j = 0; j < 4; j++) {
            float num = 0;
            for(int k = 0; k < 4 ; k++) {
                num += mat[i][k] * rot_mtx[k][j];
            }
            ret_mtx[i][j]=num;
        }
    }

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            mat[i][j] = ret_mtx[i][j];
        }
    }
}

//    eye: position of camera in world space
// center: where you want to look at, in world space
//     up: Where is up
void mat4_look_at(mat4 mat, Vect3 eye, Vect3 center, Vect3 up) {
    Vect3 f = vect3_normalise(vect3_sub(center, eye));
    Vect3 s = vect3_normalise(vect3_cross(up, f));
    Vect3 u = vect3_cross(f, s);

    mat[0][0] = s.x;
    mat[1][0] = s.y;
    mat[2][0] = s.z;

    mat[0][1] = u.x;
    mat[1][1] = u.y;
    mat[2][1] = u.z;

    mat[0][2] = f.x;
    mat[1][2] = f.y;
    mat[2][2] = f.z;

    mat[3][0] = vect3_dot(s, eye);
    mat[3][1] = vect3_dot(u, eye);
    mat[3][2] = vect3_dot(f, eye);
}

void mat4_perspective(mat4 mat, float angle, float aspect, float znear, float zfar) {
    float tan_half_angle = (float)tan(angle / 2);

    mat[0][0] = 1 / (aspect * tan_half_angle);
    mat[1][1] = 1 / tan_half_angle;
    mat[2][2] = -(zfar + znear) / (zfar - znear);
    mat[2][3] = -1;
    mat[3][2] = -(2 * zfar * znear) / (zfar - znear);
}

void mat4_transform(mat4 mat, Vect3 v) {
    mat4 ret_mtx;
     for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            ret_mtx[i][j] = mat[i][j];
        }
    }

    ret_mtx[3][0] = mat[0][0] * v.x + mat[1][0] * v.y + mat[2][0] * v.z + mat[3][0];
    ret_mtx[3][1] = mat[0][1] * v.x + mat[1][1] * v.y + mat[2][1] * v.z + mat[3][1];
    ret_mtx[3][2] = mat[0][2] * v.x + mat[1][2] * v.y + mat[2][2] * v.z + mat[3][2];
    ret_mtx[3][3] = mat[0][3] * v.x + mat[1][3] * v.y + mat[2][3] * v.z + mat[3][3];

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            mat[i][j] = ret_mtx[i][j];
        }
    }
}


float degtorad(float degrees) {
    return (float)(degrees * 3.1415) / 180;
}

int main(void) {

    // Init SDL
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL 2", "FATAL: Could not start SDL 2!", NULL);
        return -1;
    }

    if (SDL_Vulkan_LoadLibrary(NULL) != 0) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL 2", "FATAL: Could not load Vulkan library!", NULL);
        return -1;
    }

    // Create Window
    SDL_Window *window = SDL_CreateWindow("Toy Vulkan Renderer", 0, 0, 800, 600, SDL_WINDOW_VULKAN);

    // Init Vulkan

    // Create instance

    unsigned int extension_count = 0;
    if (SDL_Vulkan_GetInstanceExtensions(window, &extension_count, NULL) == SDL_FALSE) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL 2", "FATAL: Could not get window extensions count!", NULL);
        goto cleanup;
    }

    VkInstance vk_instance = VK_NULL_HANDLE;
    const char **extension_names = malloc(sizeof(char*) * extension_count);
    if (!extension_names) {
        goto cleanup;
    }
    if (SDL_Vulkan_GetInstanceExtensions(window, &extension_count, extension_names) == SDL_FALSE) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL 2", "FATAL: Could not get window extensions!", NULL);
        goto cleanup_instance;
    }

    VkInstanceCreateInfo vk_instance_create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &(VkApplicationInfo) {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = "vk_renderer",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName = "vk_renderer",
            .engineVersion = VK_MAKE_API_VERSION(1, 1, 0, 0),
            .apiVersion = VK_API_VERSION_1_0,
        },
        .enabledExtensionCount = extension_count,
        .ppEnabledExtensionNames = extension_names,
    };

    VkResult error = vkCreateInstance(&vk_instance_create_info, NULL, &vk_instance);
    if (error != VK_SUCCESS) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Could not create Vulkan instance!", NULL);
        printf("Error: %i\n", error);
        goto cleanup_instance;
    }

    // Create Surface
    VkSurfaceKHR vk_surface = {};
    if (SDL_Vulkan_CreateSurface(window, vk_instance, &vk_surface) == SDL_FALSE) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to create surface!", NULL);
        goto cleanup_instance;
    }

    // Create phsical device

    uint32_t device_count = 0;
    error = vkEnumeratePhysicalDevices(vk_instance, &device_count, NULL);
    if (error != VK_SUCCESS) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to get number of physical devices!", NULL);
        printf("Error: %i\n", error);
        goto cleanup_surface;
    }

    if (device_count == 0) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to find device with Vulkan support!", NULL);
        goto cleanup_surface;
    }

    VkPhysicalDevice *kv_physical_devices = malloc(sizeof(VkPhysicalDevice) * device_count);
    if (!extension_names) {
        goto cleanup_surface;
    }
    error = vkEnumeratePhysicalDevices(vk_instance, &device_count, kv_physical_devices);
    if (error != VK_SUCCESS) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to get physical devices!", NULL);
        goto cleanup_phyical_devices;
    }

    uint32_t device_idx = 0;
    uint32_t device_score = 0;
    uint32_t queue_family_idx = 0;

    uint32_t device_image_count = 0;
    VkPhysicalDeviceProperties device_properties = {};
    VkPhysicalDeviceFeatures device_features = {};
    VkSurfaceFormatKHR device_surface_format = {};
    VkPresentModeKHR device_present_mode = {};
    VkExtent2D device_extent2D = {};
    VkSurfaceTransformFlagBitsKHR device_pre_transform = {};
    for (uint32_t i = 0; i < device_count; i++) {
        uint32_t score = 0;

        vkGetPhysicalDeviceProperties(kv_physical_devices[i], &device_properties);
        vkGetPhysicalDeviceFeatures(kv_physical_devices[i], &device_features);

        // Check device can draw geometry
        if (!device_features.geometryShader) {
            continue;
        }

        // Bonus points for dedicated GPU
        score += device_properties.limits.maxImageDimension2D;
        if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 1000;
        }

        // Check for and get graphics queue
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(kv_physical_devices[i], &queue_family_count, NULL);
        if (queue_family_count == 0) {
            continue;
        }

        VkQueueFamilyProperties *queue_families  = malloc(sizeof(VkQueueFamilyProperties) * queue_family_count);
        if (!queue_families) {
            continue;
        }
        vkGetPhysicalDeviceQueueFamilyProperties(kv_physical_devices[i], &queue_family_count, queue_families);
        if (error != VK_SUCCESS) {
            free(queue_families);
            continue;
        }

        bool found = false;
        for (uint32_t j = 0; j < queue_family_count; j++) {
            if (queue_families[j].queueFlags > 0 && queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                queue_family_idx = j;
                found = true;
                break;
            }
        }
        free(queue_families);
        if (!found) {
            continue;
        }

        // Check for surface support
        VkBool32 has_surface_support = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(kv_physical_devices[i], queue_family_idx, vk_surface, &has_surface_support);
        if (has_surface_support == VK_FALSE) {
            continue;
        }

        // Check for swapchain extension support
        uint32_t device_extension_count = 0;
        vkEnumerateDeviceExtensionProperties(kv_physical_devices[i], NULL, &device_extension_count, NULL);
        if (device_extension_count == 0) {
            continue;
        }
        VkExtensionProperties *device_extention_properties  = malloc(sizeof(VkExtensionProperties) * device_extension_count);
        if (!device_extention_properties) {
            continue;
        }
        vkEnumerateDeviceExtensionProperties(kv_physical_devices[i], NULL, &device_extension_count, device_extention_properties);
        if (error != VK_SUCCESS) {
            free(device_extention_properties);
            continue;
        }

        found = false;
        for (uint32_t j = 0; j < device_extension_count; j++) {
            if (SDL_strncmp(device_extention_properties[j].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME, sizeof(VK_KHR_SWAPCHAIN_EXTENSION_NAME)) == 0) {
                found = true;
                break;
            }
        }
        free(device_extention_properties);
        if (!found) {
            continue;
        }

        // Check surface capabilities
        VkSurfaceCapabilitiesKHR device_surface_capabilities;
        error = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(kv_physical_devices[i], vk_surface, &device_surface_capabilities);
        if (error != VK_SUCCESS) {
            continue;
        }
        if (device_surface_capabilities.currentExtent.width == UINT32_MAX) {
            device_extent2D = device_surface_capabilities.currentExtent;
        } else {
            device_extent2D.height = window_height;
            device_extent2D.width = window_width;

            device_extent2D.height = SDL_clamp(device_extent2D.height, device_surface_capabilities.minImageExtent.height, device_surface_capabilities.maxImageExtent.height);
            device_extent2D.width = SDL_clamp(device_extent2D.width, device_surface_capabilities.minImageExtent.width, device_surface_capabilities.maxImageExtent.width);
        }
        if (device_surface_capabilities.maxImageCount == 0) {
            continue;
        }
        device_image_count = device_surface_capabilities.minImageCount + 1;
        if (device_image_count > device_surface_capabilities.maxImageCount) {
            device_image_count = device_surface_capabilities.maxImageCount;
        }
        device_pre_transform = device_surface_capabilities.currentTransform;

        uint32_t device_surface_format_count = 0;
        error = vkGetPhysicalDeviceSurfaceFormatsKHR(kv_physical_devices[i], vk_surface, &device_surface_format_count, NULL);
        if (error != VK_SUCCESS) {
            continue;
        }
        if (device_surface_format_count == 0) {
            continue;
        }
        VkSurfaceFormatKHR *device_surface_formats = malloc(sizeof(VkSurfaceFormatKHR) * device_surface_format_count);
        error = vkGetPhysicalDeviceSurfaceFormatsKHR(kv_physical_devices[i], vk_surface, &device_surface_format_count, device_surface_formats);
        if (error != VK_SUCCESS) {
            free(device_surface_formats);
            continue;
        }

        device_surface_format = device_surface_formats[0];
        for (uint32_t j = 0; j < device_surface_format_count; j++) {
            if (device_surface_formats[j].format == VK_FORMAT_B8G8R8A8_SRGB && device_surface_formats[j].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                device_surface_format = device_surface_formats[j];
                break;
            }
        }
        free(device_surface_formats);

        uint32_t device_surface_present_modes_count = 0;
        error = vkGetPhysicalDeviceSurfacePresentModesKHR(kv_physical_devices[i], vk_surface, &device_surface_present_modes_count, NULL);
        if (error != VK_SUCCESS) {
            continue;
        }
        if (device_surface_format_count == 0) {
            continue;
        }
        VkPresentModeKHR *device_presents_modes = malloc(sizeof(VkPresentModeKHR) * device_surface_present_modes_count);
        error = vkGetPhysicalDeviceSurfacePresentModesKHR(kv_physical_devices[i], vk_surface, &device_surface_present_modes_count, device_presents_modes);
        if (error != VK_SUCCESS) {
            free(device_presents_modes);
            continue;
        }
        device_present_mode = device_presents_modes[0];
        for (uint32_t j = 0; j < device_surface_present_modes_count; j++) {
            if (device_presents_modes[j] == VK_PRESENT_MODE_FIFO_KHR) {
                device_present_mode = device_presents_modes[j];
                break;
            }
        }

        free(device_presents_modes);

        if (score > device_score) {
            device_idx = i;
            device_score = score;
        }
    }

    if (device_score == 0) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to find suitable device!", NULL);
        goto cleanup_phyical_devices;
    }

    // Create virtual device
    const float queue_priority = 1.0f;
    const char *device_enabled_extension_names = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    VkDeviceCreateInfo vk_device_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &(VkDeviceQueueCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = queue_family_idx,
            .queueCount = 1,
            .pQueuePriorities = &queue_priority,
        },
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = &device_enabled_extension_names,
        .pEnabledFeatures = &device_features,
    };

    VkDevice vk_device = NULL;
    error = vkCreateDevice(kv_physical_devices[device_idx], &vk_device_create_info, NULL, &vk_device);
    if (error != VK_SUCCESS) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to create device!", NULL);
        goto cleanup_virtual_device;
    }

    // Get Queue
    VkQueue vk_queue = NULL;
    vkGetDeviceQueue(vk_device, queue_family_idx, 0, &vk_queue);

    // Create the Swapchain
    VkSwapchainCreateInfoKHR vk_swapchain_create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = vk_surface,
        .minImageCount = device_image_count,
        .imageFormat = device_surface_format.format,
        .imageColorSpace = device_surface_format.colorSpace,
        .imageExtent = device_extent2D,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
        .preTransform = device_pre_transform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = device_present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };

    VkSwapchainKHR vk_swapchain;
    error = vkCreateSwapchainKHR(vk_device, &vk_swapchain_create_info, NULL, &vk_swapchain);
    if (error != VK_SUCCESS) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to create swapchain!", NULL);
        goto cleanup_virtual_device;
    }

    // Get the swapchange images
    uint32_t vk_swapchain_image_count = 0;
    error = vkGetSwapchainImagesKHR(vk_device, vk_swapchain, &vk_swapchain_image_count, NULL);
    if (error != VK_SUCCESS) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to get swapchain image count!", NULL);
        goto cleanup_swapchain;
    }

    VkImage *vk_images = malloc(sizeof(VkImage) * vk_swapchain_image_count);
    if (!vk_images) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to allocate memory for images!", NULL);
        goto cleanup_swapchain;
    }
    error = vkGetSwapchainImagesKHR(vk_device, vk_swapchain, &vk_swapchain_image_count, vk_images);
    if (error != VK_SUCCESS) {
        free(vk_images);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to get swapchain images!", NULL);
        goto cleanup_swapchain;
    }

    VkImageView *vk_image_views = malloc(sizeof(VkImageView) * vk_swapchain_image_count);
    if (!vk_image_views) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to allocate memory for image views!", NULL);
        goto cleanup_images;
    }

    for (uint32_t i = 0; i < vk_swapchain_image_count; i++) {
        VkImageViewCreateInfo vk_image_view_create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = vk_images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = device_surface_format.format,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };
        error = vkCreateImageView(vk_device, &vk_image_view_create_info, NULL, &vk_image_views[i]);
        if (error != VK_SUCCESS) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to create image view!", NULL);
            goto cleanup_imageviews;
        }
    }

    // Load and Create shaders
    char *base_path = SDL_GetBasePath();
    char shader_path[256];

    sprintf(shader_path, "%s%s", base_path, "shaders/vert_shader.spv");
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
    error = vkCreateShaderModule(vk_device, &vert_shader_create_info, NULL, &vert_shader_module);
    if (error != VK_SUCCESS) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to create vert shader!", NULL);
        goto cleanup_vert;
    }

    sprintf(shader_path, "%s%s", base_path, "shaders/frag_shader.spv");
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
    error = vkCreateShaderModule(vk_device, &frag_shader_create_info, NULL, &frag_shader_module);
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
        .vertexAttributeDescriptionCount = 2,
        .pVertexAttributeDescriptions = (VkVertexInputAttributeDescription[]) {
            {
                .binding = 0,
                .location = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(Vertex, pos),
            },
            {
                .binding = 0,
                .location = 1,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset = offsetof(Vertex, color),
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
        .cullMode = VK_CULL_MODE_BACK_BIT,
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
        .width = (float)device_extent2D.width,
        .height = (float)device_extent2D.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    VkRect2D vk_scissor = {
        .extent = device_extent2D,
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
        .bindingCount = 1,
        .pBindings = &(VkDescriptorSetLayoutBinding) {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = NULL,
        },
    };

    VkDescriptorSetLayout camera_descriptor_set_layout = {};
    error = vkCreateDescriptorSetLayout(vk_device, &camera_descriptor_set_create_info, NULL, &camera_descriptor_set_layout);
    if (error != VK_SUCCESS) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to create camera descriptor set!", NULL);
        exit(0);
    }

    VkBuffer camera_data_buffer = {};
    VkDeviceMemory camera_data_buffer_memory = {};
    create_vkbuffer(vk_device, kv_physical_devices[device_idx], sizeof(CameraBuffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &camera_data_buffer, &camera_data_buffer_memory);

    void *camera_data_ptr;
    vkMapMemory(vk_device, camera_data_buffer_memory, 0, sizeof(CameraBuffer), 0, &camera_data_ptr);

    // Create Descriptor pool
    VkDescriptorPoolCreateInfo descriptor_pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = 1,
        .pPoolSizes = &(VkDescriptorPoolSize) {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
        },
        .maxSets = 1,
    };

    VkDescriptorPool vk_descriptor_pool = {};
    error = vkCreateDescriptorPool(vk_device, &descriptor_pool_create_info, NULL, &vk_descriptor_pool);
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
    error = vkAllocateDescriptorSets(vk_device, &descriptor_set_allocate_info, &vk_descriptor_sets);

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
    vkUpdateDescriptorSets(vk_device, 1, &write_descriptor_set, 0, NULL);

    // Create the Pipeline layout
    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &camera_descriptor_set_layout,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = NULL,
    };

    VkPipelineLayout vk_pipieline_layout = {};
    error = vkCreatePipelineLayout(vk_device, &pipeline_layout_create_info, NULL, &vk_pipieline_layout);
    if (error != VK_SUCCESS) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to create pipeline layput!", NULL);
        goto cleanup_pipeline;
    }

    // Create render passes
    VkRenderPassCreateInfo render_pass_create_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &(VkAttachmentDescription) {
            .format = device_surface_format.format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        },
        .subpassCount = 1,
        .pSubpasses = &(VkSubpassDescription) {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &(VkAttachmentReference) {
                .attachment = 0,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
        },
        .dependencyCount = 1,
        .pDependencies = &(VkSubpassDependency) {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        }
    };

    VkRenderPass vk_render_pass = {};
    error = vkCreateRenderPass(vk_device, &render_pass_create_info, NULL, &vk_render_pass);
    if (error != VK_SUCCESS) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to create render pass!", NULL);
        goto cleanup_renderpass;
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
        .pDepthStencilState = NULL,
        .pColorBlendState = &color_blend_state_create_info,
        .pDynamicState = &dynamic_state_create_info,
        .layout = vk_pipieline_layout,
        .renderPass = vk_render_pass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1,
    };

    VkPipeline vk_graphics_pipeline = {};
    error = vkCreateGraphicsPipelines(vk_device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, NULL, &vk_graphics_pipeline);
    if (error != VK_SUCCESS) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to create graphics pipeline!", NULL);
        goto vk_graphics_pipeline;
    }

    // Create FromeBuffers
    VkFramebuffer *vk_frame_buffers = malloc(sizeof(VkFramebuffer) * device_image_count);
    if (!vk_frame_buffers) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to allocate memory for frame buffers!", NULL);
        goto vk_frame_buffers;
    }

    for (uint32_t i = 0; i < device_image_count; i ++) {
        VkFramebufferCreateInfo frame_buffer_create_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = vk_render_pass,
            .attachmentCount = 1,
            .pAttachments = (VkImageView[]) {
                vk_image_views[i],
            },
            .width = device_extent2D.width,
            .height = device_extent2D.height,
            .layers = 1,
        };

        error = vkCreateFramebuffer(vk_device, &frame_buffer_create_info, NULL, &vk_frame_buffers[i]);
        if (error != VK_SUCCESS) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to create frame buffers!", NULL);
            goto vk_image_buffers;
        }
    }

    // Create command pool
    VkCommandPoolCreateInfo command_pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queue_family_idx,
    };

    VkCommandPool vk_command_pool = {};
    error = vkCreateCommandPool(vk_device, &command_pool_create_info, NULL, &vk_command_pool);
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
    error = vkAllocateCommandBuffers(vk_device, &command_buffer_create_info, &vk_command_buffer);
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
    vkCreateSemaphore(vk_device, &semaphoreInfo, NULL, &vk_image_available_semaphore);
    vkCreateSemaphore(vk_device, &semaphoreInfo, NULL, &vk_render_finished_semaphore);

    // Create current frame Fence
    VkFenceCreateInfo fence_create_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    VkFence current_frame_fence;
    error = vkCreateFence(vk_device, &fence_create_info, NULL, &current_frame_fence);
    if (error != VK_SUCCESS) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vulkan", "FATAL: Failed to create current frame fence!", NULL);
        goto vk_command_buffer;
    }

    // Load vertex data
    uint32_t total_vertex_count = 4;
    Vertex traingle_vertex_data[] = {
        {{-0.5, -0.5}, {1.0, 0.0, 0.0, 1.0}},
        {{0.5, -0.5},  {0.0, 1.0, 0.0, 1.0}},
        {{0.5, 0.5}, {0.0, 0.0, 1.0, 1.0}},
        {{-0.5, 0.5}, {0.0, 1.0, 0.0, 1.0}},
    };

    // Staging
    VkBuffer src_vk_vertex_buffer;
    VkDeviceMemory src_vertext_buffer_memory;
    create_vkbuffer(vk_device, kv_physical_devices[device_idx], sizeof(Vertex) * total_vertex_count, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &src_vk_vertex_buffer, &src_vertext_buffer_memory);

    void *data;
    vkMapMemory(vk_device, src_vertext_buffer_memory, 0, sizeof(Vertex) * total_vertex_count, 0, &data);
    memcpy(data, traingle_vertex_data, sizeof(Vertex) * total_vertex_count);
    vkUnmapMemory(vk_device, src_vertext_buffer_memory);

    // Dest
    VkBuffer vk_vertex_buffer;
    VkDeviceMemory vertext_buffer_memory;
    create_vkbuffer(vk_device, kv_physical_devices[device_idx], sizeof(Vertex) * total_vertex_count, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vk_vertex_buffer, &vertext_buffer_memory);

    // Copy Src to dest
    // Can move to seperate command pool / buffer
    VkCommandBufferAllocateInfo copy_cmd_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool = vk_command_pool,
        .commandBufferCount = 1,
    };

    VkCommandBuffer copy_cmd_buffer;
    vkAllocateCommandBuffers(vk_device, &copy_cmd_buffer_info, &copy_cmd_buffer);

    VkCommandBufferBeginInfo copy_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    vkBeginCommandBuffer(copy_cmd_buffer, &copy_begin_info);

    VkBufferCopy buffer_copy_cmd = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = sizeof(Vertex) * total_vertex_count,
    };
    vkCmdCopyBuffer(copy_cmd_buffer, src_vk_vertex_buffer, vk_vertex_buffer, 1, &buffer_copy_cmd);

    vkEndCommandBuffer(copy_cmd_buffer);

    VkSubmitInfo copy_submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &copy_cmd_buffer,
    };
    vkQueueSubmit(vk_queue, 1, &copy_submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(vk_queue);

    vkDestroyBuffer(vk_device, src_vk_vertex_buffer, NULL);
    vkFreeMemory(vk_device, src_vertext_buffer_memory, NULL);

    // Load Index Buffer
    uint32_t total_index_count = 6;
    uint16_t traingle_index_data[] = {
        0, 1, 2, 2, 3, 0,
    };

    // Staging
    VkBuffer src_vk_index_buffer;
    VkDeviceMemory src_index_buffer_memory;
    create_vkbuffer(vk_device, kv_physical_devices[device_idx], sizeof(uint16_t) * total_index_count, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &src_vk_index_buffer, &src_index_buffer_memory);

    vkMapMemory(vk_device, src_index_buffer_memory, 0, sizeof(uint16_t) * total_index_count, 0, &data);
    memcpy(data, traingle_index_data, sizeof(uint16_t) * total_index_count);
    vkUnmapMemory(vk_device, src_index_buffer_memory);

    // Dest
    VkBuffer vk_index_buffer;
    VkDeviceMemory index_buffer_memory;
    create_vkbuffer(vk_device, kv_physical_devices[device_idx], sizeof(uint16_t) * total_index_count, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vk_index_buffer, &index_buffer_memory);

    // Copy Src to dest
    // Can move to seperate command pool / buffer
    VkCommandBufferAllocateInfo index_copy_cmd_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool = vk_command_pool,
        .commandBufferCount = 1,
    };

    vkAllocateCommandBuffers(vk_device, &index_copy_cmd_buffer_info, &copy_cmd_buffer);

    VkCommandBufferBeginInfo index_copy_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    vkBeginCommandBuffer(copy_cmd_buffer, &index_copy_begin_info);

    VkBufferCopy index_buffer_copy_cmd = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = sizeof(uint16_t) * total_index_count,
    };
    vkCmdCopyBuffer(copy_cmd_buffer, src_vk_index_buffer, vk_index_buffer, 1, &index_buffer_copy_cmd);

    vkEndCommandBuffer(copy_cmd_buffer);

    VkSubmitInfo index_copy_submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &copy_cmd_buffer,
    };
    vkQueueSubmit(vk_queue, 1, &index_copy_submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(vk_queue);

    vkDestroyBuffer(vk_device, src_vk_index_buffer, NULL);
    vkFreeMemory(vk_device, src_index_buffer_memory, NULL);

    // Main loop
    SDL_Event event;

    Uint32 lastTime = SDL_GetTicks();
    ns = 1000.0 / max_ticks;
    Uint32 timer = SDL_GetTicks();
    double delta = 0;
    int32_t fps = 0;
    int32_t tick = 0;
    uptime = 0;

    //Uint32 camera_last_update = lastTime;

    float rot = 0;
    bool running = true;
    while (running) {
        Uint32 now = SDL_GetTicks();
        delta += (now - lastTime) / ns;
        lastTime = now;

        while (delta >= 1) {

            // Update

            while(SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    running = false;
                    break;
                }

                // Event
            }

            tick++;
            delta--;
        }
        fps++;

        // Render
        vkWaitForFences(vk_device, 1, &current_frame_fence, VK_TRUE, UINT64_MAX);
        vkResetFences(vk_device, 1, &current_frame_fence);

        // Create CameraBuffer
        CameraBuffer camera_bufffer = {
            .model = {
                {1, 0, 0, 0},
                {0, 1, 0, 0},
                {0, 0, 1, 0},
                {0, -0.5, 0, 1},
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
        mat4_rotate(camera_bufffer.model,  degtorad(rot), (Vect3){0, 1, 1});
        mat4_look_at(camera_bufffer.view, (Vect3){2, 2, 2}, (Vect3){0, 0, 0,}, (Vect3){0, 0, 1});
        mat4_perspective(camera_bufffer.proj, degtorad(45), (float)device_extent2D.width / (float)device_extent2D.height, (float)0.1, (float)10.0);
        camera_bufffer.proj[1][1] *= -1;

        memcpy(camera_data_ptr, &camera_bufffer, sizeof(CameraBuffer));

        uint32_t imageIndex;
        vkAcquireNextImageKHR(vk_device, vk_swapchain, UINT64_MAX, vk_image_available_semaphore, VK_NULL_HANDLE, &imageIndex);

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
                .extent = device_extent2D,
            },
            .clearValueCount = 1,
            .pClearValues = &(VkClearValue) {
                .color = {
                    .float32 = {0.0f, 0.0f, 0.0f, 0.0f},
                },
            },
        };

        // Record Render Pass
        vkCmdBeginRenderPass(vk_command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(vk_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_graphics_pipeline);

        vkCmdSetViewport(vk_command_buffer, 0, 1, &vk_viewport);
        vkCmdSetScissor(vk_command_buffer, 0, 1, &vk_scissor);

        vkCmdBindPipeline(vk_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_graphics_pipeline);

        VkBuffer vertexBuffers[] = {vk_vertex_buffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(vk_command_buffer, 0, 1, vertexBuffers, offsets);

        vkCmdBindIndexBuffer(vk_command_buffer, vk_index_buffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdBindDescriptorSets(vk_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipieline_layout, 0, 1, &vk_descriptor_sets, 0, NULL);

        vkCmdDrawIndexed(vk_command_buffer, total_index_count, 1, 0, 0, 0);

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

        error = vkQueueSubmit(vk_queue, 1, &submit_info, current_frame_fence);
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
                vk_swapchain,
            },
            .pImageIndices = &imageIndex,
            .pResults = NULL,
        };

        vkQueuePresentKHR(vk_queue, &present_info);


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
//vk_fence:
    vkDestroyBuffer(vk_device, vk_vertex_buffer, NULL);
    vkDestroyFence(vk_device, current_frame_fence, NULL);
vk_command_buffer:
    vkFreeCommandBuffers(vk_device, vk_command_pool, 1, &vk_command_buffer);
vk_command_pool:
    vkDestroyCommandPool(vk_device, vk_command_pool, NULL);
vk_image_buffers:
    for (uint32_t i = 0; i < device_image_count; i ++) {
        //free(vk_frame_buffers[i]);
    }
    free(vk_frame_buffers);
vk_frame_buffers:
    vkDestroyPipeline(vk_device, vk_graphics_pipeline, NULL);
vk_graphics_pipeline:
    vkDestroyRenderPass(vk_device, vk_render_pass, NULL);
cleanup_renderpass:
    vkDestroyPipelineLayout(vk_device, vk_pipieline_layout, NULL);
cleanup_pipeline:
    vkDestroyShaderModule(vk_device, frag_shader_module, NULL);
cleanup_frag:
    free(frag_shader);
    vkDestroyShaderModule(vk_device, vert_shader_module, NULL);
cleanup_vert:
    free(vert_shader);
cleanup_imageviews:
    for (uint32_t i = 0; i < vk_swapchain_image_count; i++) {
        vkDestroyImageView(vk_device, vk_image_views[i], NULL);
    }
    free(vk_image_views);
cleanup_images:
    for (uint32_t i = 0; i < vk_swapchain_image_count; i++) {
        vkDestroyImage(vk_device, vk_images[i], NULL);
    }
    free(vk_images);
cleanup_swapchain:
    //vkDestroySwapchainKHR(vk_device, vk_swapchain, NULL);
cleanup_virtual_device:
    vkDestroyDevice(vk_device, NULL);
cleanup_phyical_devices:
    free(kv_physical_devices);
cleanup_surface:
    vkDestroySurfaceKHR(vk_instance, vk_surface, NULL);
    SDL_DestroyWindowSurface(window);
cleanup_instance:
    vkDestroyInstance(vk_instance, NULL);
    free(extension_names);
cleanup:
    SDL_DestroyWindow(window);
    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();

    return 0;
}
