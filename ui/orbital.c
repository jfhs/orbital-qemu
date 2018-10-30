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

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "ui/console.h"
#include "ui/vk-helpers.h"
#include "qemu/thread.h"
#include "qemu/error-report.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define IMGUI_IMPL_API
#include "imgui/cimgui_auto.h"
#include "imgui/imgui_impl_sdl.h"
#include "imgui/imgui_impl_vulkan.h"

// Configuration
#define ORBITAL_WIDTH 1280
#define ORBITAL_HEIGHT 720

typedef struct OrbitalUI {
    /* vulkan */
    VulkanState vk_state;
    /* sdl */
    QemuThread sdl_thread;
    SDL_Window* sdl_window;
    /* imgui */
    ImGui_ImplVulkanH_WindowData imgui_WindowData;
} OrbitalUI;

// Global state
OrbitalUI ui;


static
void* orbital_display_main(void* arg)
{
    VulkanState* vks = &ui.vk_state;
    SDL_Event evt;
    int err, flags;
    bool quit;
    uint32_t count;

    err = SDL_InitSubSystem(SDL_INIT_VIDEO);
    if (err) {
        printf("SDL_InitSubSystem failed: %s\n", SDL_GetError());
        return NULL;
    }

    err = SDL_Vulkan_LoadLibrary("libvulkan-1.dll");
    if (err) {
        printf("SDL_Vulkan_LoadLibrary failed: %s\n", SDL_GetError());
        return NULL;
    }

    flags = 0;
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
    if (!SDL_Vulkan_CreateSurface(ui.sdl_window, vks->instance, &vks->surface)) {
        printf("SDL_Vulkan_CreateSurface failed: %s\n", SDL_GetError());
        return NULL;
    }
    vk_init_device(vks);

    // Create framebuffers
    int w, h;
    SDL_GetWindowSize(ui.sdl_window, &w, &h);
    ImGui_ImplVulkanH_WindowData* wd = &ui.imgui_WindowData;
    SetupVulkanWindowData(wd, surface, w, h);

    // Setup Dear ImGui binding
    igCreateContext();
    ImGuiIO& io = igGetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

    // Setup SDL binding
    ImGui_ImplSDL2_InitForVulkan(window);

    // Setup Vulkan binding
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = g_Instance;
    init_info.PhysicalDevice = g_PhysicalDevice;
    init_info.Device = g_Device;
    init_info.QueueFamily = g_QueueFamily;
    init_info.Queue = g_Queue;
    init_info.PipelineCache = g_PipelineCache;
    init_info.DescriptorPool = g_DescriptorPool;
    init_info.Allocator = g_Allocator;
    init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info, wd->RenderPass);

    // Setup style
    igStyleColorsDark();   

     // Upload Fonts
    {
        // Use any command queue
        VkCommandPool command_pool = wd->Frames[wd->FrameIndex].CommandPool;
        VkCommandBuffer command_buffer = wd->Frames[wd->FrameIndex].CommandBuffer;

        err = vkResetCommandPool(g_Device, command_pool, 0);
        check_vk_result(err);
        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(command_buffer, &begin_info);
        check_vk_result(err);

        ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

        VkSubmitInfo end_info = {};
        end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        end_info.commandBufferCount = 1;
        end_info.pCommandBuffers = &command_buffer;
        err = vkEndCommandBuffer(command_buffer);
        check_vk_result(err);
        err = vkQueueSubmit(g_Queue, 1, &end_info, VK_NULL_HANDLE);
        check_vk_result(err);

        err = vkDeviceWaitIdle(g_Device);
        check_vk_result(err);
        ImGui_ImplVulkan_InvalidateFontUploadObjects();
    }   
    
    quit = false;
    while (!quit) {
        // Events
        while (SDL_PollEvent(&evt) == 1) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (evt.type == SDL_QUIT) {
                quit = true;
            }
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_RESIZED &&
                event.window.windowID == SDL_GetWindowID(window)) {
                ImGui_ImplVulkanH_CreateWindowDataSwapChainAndFramebuffer(
                    g_PhysicalDevice, g_Device, &g_WindowData, g_Allocator,
                    (int)event.window.data1, (int)event.window.data2);
            }
        }

        // Frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        igNewFrame();

        // Window
        {
            static float f = 0.0f;
            static int counter = 0;

            igBegin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

            igText("This is some useful text.");               // Display some text (you can use a format strings too)
            igCheckbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
            igCheckbox("Another Window", &show_another_window);

            igSliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f    
            igColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if (igButton("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            igSameLine();
            igText("counter = %d", counter);

            igText("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / igGetIO().Framerate, igGetIO().Framerate);
            igEnd();
        }


        // Rendering
        igRender();
        memcpy(&wd->ClearValue.color.float32[0], &clear_color, 4 * sizeof(float));
		FrameRender(wd);
        FramePresent(wd);
    }

    err = vkDeviceWaitIdle(g_Device);
    check_vk_result(err);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    igDestroyContext();
    SDL_DestroyWindow(window);
    CleanupVulkan();
    SDL_Quit();

    return NULL;    
}

static void orbital_display_early_init(DisplayOptions *o)
{
}

static void orbital_display_init(DisplayState *ds, DisplayOptions *o)
{
    qemu_thread_create(&ui.sdl_thread, "sdl_thread",
        orbital_display_main, NULL, QEMU_THREAD_JOINABLE);
}

static QemuDisplay qemu_display_orbital = {
    .type       = DISPLAY_TYPE_ORBITAL,
    .early_init = orbital_display_early_init,
    .init       = orbital_display_init,
};

static void register_orbital(void)
{
    qemu_display_register(&qemu_display_orbital);
}

type_init(register_orbital);
