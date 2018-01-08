/*
 * QEMU Vulkan helpers
 *
 * Copyright (c) 2017 Alexandro Sanchez Bach
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "ui/vk-helpers.h"
#include "qemu/osdep.h"
#include "qemu/error-report.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

/* helpers */
static
void vk_find_graphics_queue(VulkanState* s)
{
    uint32_t graphicsQueueNodeIndex = UINT32_MAX;
    uint32_t presentQueueNodeIndex = UINT32_MAX;
    uint32_t i;

    for (i = 0; i < s->queue_count; i++) {
        if ((s->queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
            VkBool32 supportsPresent;
            vkGetPhysicalDeviceSurfaceSupportKHR(s->gpu, i, s->surface, &supportsPresent);
            if (graphicsQueueNodeIndex == UINT32_MAX) {
                graphicsQueueNodeIndex = i;
            }
            if (supportsPresent == VK_TRUE) {
                graphicsQueueNodeIndex = i;
                presentQueueNodeIndex = i;
                break;
            }
        }
    }
    if (presentQueueNodeIndex == UINT32_MAX) {
        for (i = 0; i < s->queue_count; ++i) {
            VkBool32 supportsPresent;
            vkGetPhysicalDeviceSurfaceSupportKHR(s->gpu, i, s->surface, &supportsPresent);
            if (supportsPresent == VK_TRUE) {
                presentQueueNodeIndex = i;
                break;
            }
        }
    }
    if (graphicsQueueNodeIndex == UINT32_MAX || presentQueueNodeIndex == UINT32_MAX) {
        error_report("vk_init_device: Could not find a graphics and a present queue\n");
        return;
    }
    if (graphicsQueueNodeIndex != presentQueueNodeIndex) {
        error_report("vk_init_device: Could not find a common graphics and a present queue\n");
        return;
    }
    s->graphics_queue_node_index = graphicsQueueNodeIndex;
}

/* interface */
void vk_init_instance(VulkanState* s, uint32_t extCount, const char **extNames)
{
    VkApplicationInfo applicationInfo = {};
    VkInstanceCreateInfo instanceInfo = {};
    VkResult vkr;

    // Set Vulkan instance layers and extensions
    const char* instanceLayerNames[] = {
        "VK_LAYER_LUNARG_standard_validation",
    };
    
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pNext = NULL;
    applicationInfo.pApplicationName = "QEMU";
    applicationInfo.applicationVersion = 1;
    applicationInfo.pEngineName = "qemu-vk";
    applicationInfo.engineVersion = 1;
    applicationInfo.apiVersion = VK_API_VERSION_1_0;

    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pNext = NULL;
    instanceInfo.flags = 0;
    instanceInfo.pApplicationInfo = &applicationInfo;
    instanceInfo.enabledLayerCount = countof(instanceLayerNames);
    instanceInfo.ppEnabledLayerNames = instanceLayerNames;
    instanceInfo.enabledExtensionCount = extCount;
    instanceInfo.ppEnabledExtensionNames = extNames;
    vkr = vkCreateInstance(&instanceInfo, NULL, &s->instance);
    if (vkr != VK_SUCCESS) {
        error_report("vkCreateInstance failed with code %d", vkr);
        return;
    }
}

void vk_init_device(VulkanState* s)
{
    VkResult vkr;

    uint32_t gpu_count;
    vkr = vkEnumeratePhysicalDevices(s->instance, &gpu_count, NULL);
    if (vkr != VK_SUCCESS) {
        error_report("vkEnumeratePhysicalDevices failed with code %d", vkr);
        return;
    }
    assert(gpu_count >= 1);
    VkPhysicalDevice *physical_devices = malloc(sizeof(VkPhysicalDevice) * gpu_count);
    vkr = vkEnumeratePhysicalDevices(s->instance, &gpu_count, physical_devices);
    s->gpu = physical_devices[0];
    free(physical_devices);

    vkGetPhysicalDeviceFeatures(s->gpu, &s->gpu_features);
    vkGetPhysicalDeviceProperties(s->gpu, &s->gpu_props);

    /* queue */
    vkGetPhysicalDeviceQueueFamilyProperties(s->gpu, &s->queue_count, NULL);
    s->queue_props = (VkQueueFamilyProperties *)malloc(s->queue_count * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(s->gpu, &s->queue_count, s->queue_props);
    assert(s->queue_count >= 1);
    vk_find_graphics_queue(s);

    float queue_priorities[1] = {0.0};
    VkDeviceQueueCreateInfo deviceQueue = {};
    deviceQueue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    deviceQueue.pNext = NULL;
    deviceQueue.queueFamilyIndex = s->graphics_queue_node_index;
    deviceQueue.queueCount = 1;
    deviceQueue.pQueuePriorities = queue_priorities;

    /* device */
    static const char *deviceExtensionNames[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    VkDeviceCreateInfo deviceInfo = {};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext = NULL;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &deviceQueue;
    deviceInfo.enabledExtensionCount = countof(deviceExtensionNames);
    deviceInfo.ppEnabledExtensionNames = deviceExtensionNames;
    deviceInfo.pEnabledFeatures = NULL;
    deviceInfo.ppEnabledLayerNames = NULL;
    vkr = vkCreateDevice(s->gpu, &deviceInfo, NULL, &s->device);
    if (vkr != VK_SUCCESS) {
        error_report("vkCreateInstance failed with code %d", vkr);
        return;
    }
}
