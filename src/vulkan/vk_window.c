#include "vk_window.h"

#include <stdbool.h>

#include "src/io/memory.h"
#include "src/error/error.h"

void vk_window_create(Window *r_window, const char *p_title, size_t p_width, size_t p_height) {
    // Create SDL window, related instance and surface
    r_window->sdl_window = SDL_CreateWindow(p_title, 0, 0, p_width, p_height, SDL_WINDOW_VULKAN);

    // Get requred SDL extensions
    uint32_t extension_count = 0;
    CRASH_COND_MSG(SDL_Vulkan_GetInstanceExtensions(r_window->sdl_window, &extension_count, NULL) == SDL_FALSE, "%s", "FATAL: Could not get window extensions count!");

    const char **extension_names = mmalloc(sizeof(char*) * extension_count);
    CRASH_COND_MSG(SDL_Vulkan_GetInstanceExtensions(r_window->sdl_window, &extension_count, extension_names) == SDL_FALSE, "%s", "FATAL: Could not get window extensions names!");

    /*uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, NULL);

    VkLayerProperties *availableLayers = mmalloc(sizeof(VkLayerProperties) * layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers);

    for (uint32_t i = 0; i < layerCount; i++) {
        INFO_MSG("%s", availableLayers[i].layerName);
    }*/

    CRASH_COND_MSG(vkCreateInstance(
        &(VkInstanceCreateInfo) {
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
            .enabledLayerCount = 1,
            .ppEnabledLayerNames = &(const char *){"VK_LAYER_KHRONOS_validation"},
        },
        NULL, &r_window->vk_instance) != VK_SUCCESS,
    "%s", "FATAL: Could not create Vulkan instance!");

    CRASH_COND_MSG(SDL_Vulkan_CreateSurface(r_window->sdl_window, r_window->vk_instance, &r_window->vk_surface) == SDL_FALSE, "%s", "FATAL: Failed to create vulkan surface!");
    mfree(extension_names);

    // Find valid physical device info
    uint32_t device_count = 0;
    CRASH_COND_MSG(vkEnumeratePhysicalDevices(r_window->vk_instance, &device_count, NULL) != VK_SUCCESS, "%s", "FATAL: Failed to get number of physical devices!");
    CRASH_COND_MSG(device_count == 0, "%s", "FATAL: Failed to find device with Vulkan support!");

    VkPhysicalDevice *physical_devices = mmalloc(sizeof(VkPhysicalDevice) * device_count);
    CRASH_COND_MSG(vkEnumeratePhysicalDevices(r_window->vk_instance, &device_count, physical_devices) != VK_SUCCESS, "%s", "FATAL: Failed to get physical devices!");

    uint32_t device_idx = 0;
    uint32_t device_score = 0;
    VkPhysicalDeviceFeatures device_features = { 0 };
    VkPhysicalDeviceProperties device_properties = { 0 };
    VkSurfaceTransformFlagBitsKHR vk_surface_transform;
    VkPresentModeKHR vk_present_mode;
    for (uint32_t i = 0; i < device_count; i++) {
        uint32_t score = 0;

        vkGetPhysicalDeviceProperties(physical_devices[i], &device_properties);
        vkGetPhysicalDeviceFeatures(physical_devices[i], &device_features);

        // Check device can draw geometry
        if (!device_features.geometryShader) {
            continue;
        }

        // Check for Anisotropy
        if (!device_features.samplerAnisotropy) {
            continue;
        }
        r_window->max_sampler_anisotropy = device_properties.limits.maxSamplerAnisotropy;

        // Bonus points for dedicated GPU
        score += device_properties.limits.maxImageDimension2D;
        if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 1000;
        }

        // Check for and get graphics queue
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_devices[i], &queue_family_count, NULL);
        if (queue_family_count == 0) {
            continue;
        }

        VkQueueFamilyProperties *queue_families  = mmalloc(sizeof(VkQueueFamilyProperties) * queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_devices[i], &queue_family_count, queue_families);

        bool found = false;
        for (uint32_t j = 0; j < queue_family_count; j++) {
            if (queue_families[j].queueFlags > 0 && queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                r_window->vk_queue_index = j;
                found = true;
                break;
            }
        }
        mfree(queue_families);
        if (!found) {
            continue;
        }

        // Check for surface support
        VkBool32 has_surface_support = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physical_devices[i], r_window->vk_queue_index, r_window->vk_surface, &has_surface_support);
        if (has_surface_support == VK_FALSE) {
            continue;
        }

        // Check for swapchain extension support
        uint32_t device_extension_count = 0;
        vkEnumerateDeviceExtensionProperties(physical_devices[i], NULL, &device_extension_count, NULL);
        if (device_extension_count == 0) {
            continue;
        }
        VkExtensionProperties *device_extention_properties = mmalloc(sizeof(VkExtensionProperties) * device_extension_count);
        if (vkEnumerateDeviceExtensionProperties(physical_devices[i], NULL, &device_extension_count, device_extention_properties) != VK_SUCCESS) {
            mfree(device_extention_properties);
            continue;
        }

        found = false;
        for (uint32_t j = 0; j < device_extension_count; j++) {
            if (SDL_strncmp(device_extention_properties[j].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME, sizeof(VK_KHR_SWAPCHAIN_EXTENSION_NAME)) == 0) {
                found = true;
                break;
            }
        }
        mfree(device_extention_properties);
        if (!found) {
            continue;
        }

        // Check surface capabilities
        VkSurfaceCapabilitiesKHR device_surface_capabilities = { 0 };
        if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_devices[i], r_window->vk_surface, &device_surface_capabilities) != VK_SUCCESS) {
            continue;
        }
        if (device_surface_capabilities.currentExtent.width == UINT32_MAX) {
            r_window->vk_extent2D = device_surface_capabilities.currentExtent;
        } else {
            r_window->vk_extent2D.height = SDL_clamp(p_height, device_surface_capabilities.minImageExtent.height, device_surface_capabilities.maxImageExtent.height);
            r_window->vk_extent2D.width = SDL_clamp(p_width, device_surface_capabilities.minImageExtent.width, device_surface_capabilities.maxImageExtent.width);
        }
        if (device_surface_capabilities.maxImageCount == 0) {
            continue;
        }
        r_window->image_count = SDL_min(device_surface_capabilities.minImageCount + 1, device_surface_capabilities.maxImageCount);
        vk_surface_transform = device_surface_capabilities.currentTransform;

        uint32_t device_surface_format_count = 0;
        if (vkGetPhysicalDeviceSurfaceFormatsKHR(physical_devices[i], r_window->vk_surface, &device_surface_format_count, NULL) != VK_SUCCESS) {
            continue;
        }
        if (device_surface_format_count == 0) {
            continue;
        }
        VkSurfaceFormatKHR *device_surface_formats = mmalloc(sizeof(VkSurfaceFormatKHR) * device_surface_format_count);
        if (vkGetPhysicalDeviceSurfaceFormatsKHR(physical_devices[i], r_window->vk_surface, &device_surface_format_count, device_surface_formats) != VK_SUCCESS) {
            mfree(device_surface_formats);
            continue;
        }

        r_window->vk_surface_format = device_surface_formats[0];
        for (uint32_t j = 0; j < device_surface_format_count; j++) {
            if (device_surface_formats[j].format == VK_FORMAT_B8G8R8A8_SRGB && device_surface_formats[j].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                r_window->vk_surface_format = device_surface_formats[j];
                break;
            }
        }
        mfree(device_surface_formats);

        uint32_t device_surface_present_modes_count = 0;
        if (vkGetPhysicalDeviceSurfacePresentModesKHR(physical_devices[i], r_window->vk_surface, &device_surface_present_modes_count, NULL) != VK_SUCCESS) {
            continue;
        }
        if (device_surface_format_count == 0) {
            continue;
        }
        VkPresentModeKHR *device_presents_modes = mmalloc(sizeof(VkPresentModeKHR) * device_surface_present_modes_count);
        if (vkGetPhysicalDeviceSurfacePresentModesKHR(physical_devices[i], r_window->vk_surface, &device_surface_present_modes_count, device_presents_modes) != VK_SUCCESS) {
            mfree(device_presents_modes);
            continue;
        }
        vk_present_mode = device_presents_modes[0];
        for (uint32_t j = 0; j < device_surface_present_modes_count; j++) {
            if (device_presents_modes[j] == VK_PRESENT_MODE_FIFO_KHR) {
                vk_present_mode = device_presents_modes[j];
                break;
            }
        }
        mfree(device_presents_modes);

        // Check for depth format
        found = false;
        VkImageTiling search_tiling = VK_IMAGE_TILING_OPTIMAL;
        VkFormat search_formats[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
        VkFormatFeatureFlags search_features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
        for (int j = 0; j < 3; j++) {
            VkFormatProperties props = { 0 };
            vkGetPhysicalDeviceFormatProperties(physical_devices[i], search_formats[j], &props);

            if (search_tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & search_features) == search_features) {
                found = true;
                r_window->vk_depth_format = search_formats[j];
                break;
            }

            if (search_tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & search_features) == search_features) {
                found = true;
                r_window->vk_depth_format = search_formats[j];
                break;
            }
        }
        if (!found) {
            continue;
        }

        if (score > device_score) {
            device_idx = i;
            device_score = score;
        }
    }
    CRASH_COND_MSG(device_score == 0, "%s", "FATAL: Failed to find suitable device!");
    r_window->vk_physical_device = physical_devices[device_idx];

    mfree(physical_devices);

    // Create virtual device
    // TODO: Move to renderer?
    const float queue_priority = 1.0f;
    const char *device_enabled_extension_names = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    CRASH_COND_MSG(vkCreateDevice(r_window->vk_physical_device,
        &(VkDeviceCreateInfo){
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &(VkDeviceQueueCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = r_window->vk_queue_index,
                .queueCount = 1,
                .pQueuePriorities = &queue_priority,
            },
            .enabledExtensionCount = 1,
            .ppEnabledExtensionNames = &device_enabled_extension_names,
            .pEnabledFeatures = &device_features,
        }, NULL, &r_window->vk_device) != VK_SUCCESS,
        "%s", "FATAL: Failed to create device!");

    // Get queue
    vkGetDeviceQueue(r_window->vk_device, r_window->vk_queue_index, 0, &r_window->vk_queue);

    // Create surface swapchain
    CRASH_COND_MSG(vkCreateSwapchainKHR(r_window->vk_device,
        &(VkSwapchainCreateInfoKHR) {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = r_window->vk_surface,
            .minImageCount = r_window->image_count,
            .imageFormat = r_window->vk_surface_format.format,
            .imageColorSpace = r_window->vk_surface_format.colorSpace,
            .imageExtent = r_window->vk_extent2D,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = NULL,
            .preTransform = vk_surface_transform,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = vk_present_mode,
            .clipped = VK_TRUE,
            .oldSwapchain = VK_NULL_HANDLE,
        }, NULL, &r_window->vk_swapchain) != VK_SUCCESS,
        "%s", "FATAL: Failed to create swapchain!");

    // Get the swapchain images
    VkImage *vk_swapchain_images = mmalloc(sizeof(VkImage) * r_window->image_count);
    CRASH_COND_MSG(vkGetSwapchainImagesKHR(r_window->vk_device, r_window->vk_swapchain, &r_window->image_count, vk_swapchain_images) != VK_SUCCESS, "%s", "FATAL: Failed to get swapchain images!");

    r_window->images = mmalloc(sizeof(WindowImage) * r_window->image_count);
    for (uint32_t i = 0; i < r_window->image_count; i++) {
        r_window->images[i].vk_image = vk_swapchain_images[i];

        CRASH_COND_MSG(vkCreateImageView(r_window->vk_device,
            &(VkImageViewCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = r_window->images[i].vk_image,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = r_window->vk_surface_format.format,
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
            }, NULL, &r_window->images[i].vk_image_view) != VK_SUCCESS,
            "%s", "FATAL: Failed to create image view!");
    }

    mfree(vk_swapchain_images);
}

void vk_window_free(Window *r_window) {
    for (uint32_t i = 0; i < r_window->image_count; i++) {
        vkDestroyImageView(r_window->vk_device, r_window->images[i].vk_image_view, NULL);
    }
    mfree(r_window->images);
    vkDestroySwapchainKHR(r_window->vk_device, r_window->vk_swapchain, NULL);
    vkDestroyDevice(r_window->vk_device, NULL);
    vkDestroySurfaceKHR(r_window->vk_instance, r_window->vk_surface, NULL);
    vkDestroyInstance(r_window->vk_instance, NULL);
    SDL_DestroyWindow(r_window->sdl_window);
}
