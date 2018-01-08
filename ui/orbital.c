/*
 * QEMU-Orbital user interface
 *
 * Copyright (c) 2017-2018 Alexandro Sanchez Bach
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

#include "ui/orbital.h"
#include "ui/vk-helpers.h"
#include "qemu/osdep.h"
#include "qemu/thread.h"
#include "qemu/error-report.h"

#include <SDL2/sdl.h>
#include <SDL2/sdl_vulkan.h>
#include <vulkan/vulkan.h>

// Configuration
#define ORBITAL_WIDTH 1280
#define ORBITAL_HEIGHT 720

typedef struct OrbitalUI {
    /* vulkan */
    VulkanState vk_state;
    /* sdl */
    QemuThread sdl_thread;
    SDL_Window* sdl_window;
} OrbitalUI;

// Global state
OrbitalUI ui;

static
void* orbital_display_main(void* arg)
{
    VulkanState* vks = &ui.vk_state;
    SDL_Event evt;
    bool quit;
    uint32_t count;

    int flags = 0;
    flags |= SDL_WINDOW_RESIZABLE;
    flags |= SDL_WINDOW_VULKAN;
    ui.sdl_window = SDL_CreateWindow("Orbital",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        ORBITAL_WIDTH, ORBITAL_HEIGHT, flags);

    count = 0;
    SDL_Vulkan_GetInstanceExtensions(ui.sdl_window, &count, NULL);
    if (count == 0) {
        error_report("SDL_Vulkan_GetInstanceExtensions failed");
    }

    const char **extensionNames = (const char **)malloc((count + 1) * sizeof(char*));
    extensionNames[0] = VK_KHR_SURFACE_EXTENSION_NAME;
    SDL_Vulkan_GetInstanceExtensions(ui.sdl_window, &count, &extensionNames[1]);

    vk_init_instance(vks, count+1, extensionNames);
    if (SDL_Vulkan_CreateSurface(ui.sdl_window, vks->instance, &vks->surface) < 0) {
        printf("SDL_Vulkan_CreateSurface failed: %s\n", SDL_GetError());
    }
    vk_init_device(vks);
    
    quit = false;
    while (!quit) {
        while (SDL_PollEvent(&evt) == 1) {
            if (evt.type == SDL_QUIT) {
                quit = true;
            }
        }
        vkDeviceWaitIdle(vks->device);
    }
    return NULL;    
}

void orbital_display_init(void)
{
    qemu_thread_create(&ui.sdl_thread, "sdl_thread",
        orbital_display_main, NULL, QEMU_THREAD_JOINABLE);
}
