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

#include "orbital.h"
#include "orbital-logs.h"
#include "orbital-stats.h"
#include "orbital-procs.h"

// Configuration
#define ORBITAL_WIDTH 1280
#define ORBITAL_HEIGHT 720

typedef struct OrbitalUI {
    bool active;
    /* vulkan */
    VulkanState vk_state;
    /* sdl */
    QemuThread sdl_thread;
    SDL_Window* sdl_window;
    /* imgui */
    ImGui_ImplVulkanH_WindowData imgui_WindowData;
    struct orbital_stats_t *stats;
    struct orbital_logs_t *logs_uart;
    struct orbital_procs_t *procs;
    bool show_stats;
    bool show_uart;
    bool show_trace_cp;
    bool show_trace_icc;
    bool show_trace_samu;
    bool show_mem_gpa;
    bool show_mem_gva;
    bool show_mem_gart;
    bool show_mem_iommu;
} OrbitalUI;

// Global state
OrbitalUI ui;

bool orbital_display_active(void)
{
    return ui.active;
}

void orbital_log_uart(int index, char ch)
{
    (void)index; // Unused
    orbital_logs_logchr(ui.logs_uart, ch);
}

void orbital_log_event(int device, int component, int event)
{
    orbital_stats_log(ui.stats, device, component, event);
}

void orbital_update_cpu_procs(int cpuid, uint64_t gs, uint64_t thread_ptr, uint64_t proc_ptr, uint64_t pid, const char* name)
{
    // TODO: this is some bad code, too much copying
    orbital_procs_cpu_data data;
    data.gs = gs;
    data.thread_pointer = thread_ptr;
    data.proc_pointer = proc_ptr;
    data.pid = pid;
    if (name) {
        size_t l = strlen(name);
        memcpy(data.proc_name, name, l);
        data.proc_name[l] = 0;
    } else {
        data.proc_name[0] = 0;
    }
    orbital_procs_update(ui.procs, cpuid, data);
}

static void check_vk_result(VkResult err)
{
    if (err == 0) return;
    error_report("VkResult %d\n", err);
    assert(0);
}

static void SetupVulkanWindowData(ImGui_ImplVulkanH_WindowData* wd, VulkanState* state, int width, int height)
{
    wd->Surface = state->surface;

    // Check for WSI support
    VkBool32 res;
    vkGetPhysicalDeviceSurfaceSupportKHR(state->gpu, state->graphics_queue_node_index, wd->Surface, &res);
    if (res != VK_TRUE)
    {
        fprintf(stderr, "Error no WSI support on physical device 0\n");
        exit(-1);
    }

    // Select Surface Format
    const VkFormat requestSurfaceImageFormat[] = {
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8_UNORM,
        VK_FORMAT_R8G8B8_UNORM
    };
    const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(
        state->gpu, wd->Surface, requestSurfaceImageFormat, (size_t)ARRAYSIZE(requestSurfaceImageFormat), requestSurfaceColorSpace);

    // Select Present Mode
    VkPresentModeKHR present_modes[] = {
#ifdef IMGUI_UNLIMITED_FRAME_RATE
        VK_PRESENT_MODE_MAILBOX_KHR,
        VK_PRESENT_MODE_IMMEDIATE_KHR,
        VK_PRESENT_MODE_FIFO_KHR
#else
        VK_PRESENT_MODE_FIFO_KHR
#endif
    };
    wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(state->gpu, wd->Surface, &present_modes[0], ARRAYSIZE(present_modes));
    //printf("[vulkan] Selected PresentMode = %d\n", wd->PresentMode);

    // Create SwapChain, RenderPass, Framebuffer, etc.
    ImGui_ImplVulkanH_CreateWindowDataCommandBuffers(state->gpu, state->device, state->graphics_queue_node_index, wd, NULL);
    ImGui_ImplVulkanH_CreateWindowDataSwapChainAndFramebuffer(state->gpu, state->device, wd, NULL, width, height);
}


static void FrameRender(ImGui_ImplVulkanH_WindowData* wd, VulkanState* vks)
{
    VkResult err;

    VkSemaphore image_acquired_semaphore = wd->Frames[wd->FrameIndex].ImageAcquiredSemaphore;
    err = vkAcquireNextImageKHR(vks->device, wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
    check_vk_result(err);

    ImGui_ImplVulkanH_FrameData* fd = &wd->Frames[wd->FrameIndex];
    {
        err = vkWaitForFences(vks->device, 1, &fd->Fence, VK_TRUE, UINT64_MAX);    // wait indefinitely instead of periodically checking
        check_vk_result(err);

        err = vkResetFences(vks->device, 1, &fd->Fence);
        check_vk_result(err);
    }
    {
        err = vkResetCommandPool(vks->device, fd->CommandPool, 0);
        check_vk_result(err);
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
        check_vk_result(err);
    }
    {
        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = wd->RenderPass;
        info.framebuffer = wd->Framebuffer[wd->FrameIndex];
        info.renderArea.extent.width = wd->Width;
        info.renderArea.extent.height = wd->Height;
        info.clearValueCount = 1;
        info.pClearValues = &wd->ClearValue;
        vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    }

    // Record Imgui Draw Data and draw funcs into command buffer
    ImGui_ImplVulkan_RenderDrawData(igGetDrawData(), fd->CommandBuffer);

    // Submit command buffer
    vkCmdEndRenderPass(fd->CommandBuffer);
    {
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &image_acquired_semaphore;
        info.pWaitDstStageMask = &wait_stage;
        info.commandBufferCount = 1;
        info.pCommandBuffers = &fd->CommandBuffer;
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores = &fd->RenderCompleteSemaphore;

        err = vkEndCommandBuffer(fd->CommandBuffer);
        check_vk_result(err);
        err = vkQueueSubmit(vks->queue, 1, &info, fd->Fence);
        check_vk_result(err);
    }
}

static void FramePresent(ImGui_ImplVulkanH_WindowData* wd, VulkanState* vks)
{
    ImGui_ImplVulkanH_FrameData* fd = &wd->Frames[wd->FrameIndex];
    VkPresentInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &fd->RenderCompleteSemaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &wd->Swapchain;
    info.pImageIndices = &wd->FrameIndex;
    VkResult err = vkQueuePresentKHR(vks->queue, &info);
    check_vk_result(err);
}

static void CleanupVulkan(ImGui_ImplVulkanH_WindowData* wd, VulkanState* vks)
{
    ImGui_ImplVulkanH_DestroyWindowData(vks->instance, vks->device, wd, NULL);
    vkDestroyDescriptorPool(vks->device, vks->descriptor_pool, NULL);

    vkDestroyDevice(vks->device, NULL);
    vkDestroyInstance(vks->instance, NULL);
}

static void orbital_display_draw(OrbitalUI *ui)
{
    igBeginMainMenuBar();
    if (igBeginMenu("File", true)) {
        if (igMenuItemBool("Open kernel...", NULL, false, false))   { /* TODO */ }
        igSeparator();
        if (igMenuItemBool("Exit", NULL, false, true)) { /* TODO */ }
        igEndMenu();
    }
    if (igBeginMenu("Machine", true)) {
        if (igMenuItemBool("Run", NULL, false, false))   { /* TODO */ }
        if (igMenuItemBool("Pause", NULL, false, false)) { /* TODO */ }
        if (igMenuItemBool("Stop", NULL, false, false))  { /* TODO */ }
        igSeparator();
        if (igMenuItemBool("Load state", NULL, false, false)) { /* TODO */ }
        if (igMenuItemBool("Save state", NULL, false, false)) { /* TODO */ }
        igSeparator();
        if (igMenuItemBool("Configuration...", NULL, false, false)) { /* TODO */ }
        igEndMenu();
    }
    if (igBeginMenu("Tools", true)) {
        igMenuItemBoolPtr("Statistics", "Alt+1", &ui->show_stats, true);
        igMenuItemBoolPtr("UART Output", "Alt+2", &ui->show_uart, true);
        igSeparator();
        igMenuItemBoolPtr("CP Commands", "Alt+3", &ui->show_trace_cp, false);
        igMenuItemBoolPtr("ICC Commands", "Alt+4", &ui->show_trace_icc, false);
        igMenuItemBoolPtr("SAMU Commands", "Alt+5", &ui->show_trace_samu, false);
        igSeparator();
        igMenuItemBoolPtr("Memory Editor (GPA)", "Ctrl+1", &ui->show_mem_gpa, false);
        igMenuItemBoolPtr("Memory Editor (GVA)", "Ctrl+2", &ui->show_mem_gva, false);
        igMenuItemBoolPtr("Memory Editor (GART)", "Ctrl+3", &ui->show_mem_gart, false);
        igMenuItemBoolPtr("Memory Editor (IOMMU)", "Ctrl+4", &ui->show_mem_iommu, false);
        igEndMenu();
    }
    if (igBeginMenu("Help", true)) {
        if (igMenuItemBool("About...", NULL, false, false)) {
            /* TODO */
        }
        igEndMenu();
    }
    igEndMainMenuBar();

    if (ui->show_stats) {
        orbital_stats_draw(ui->stats, "Statistics", &ui->show_stats);
    }

    if (ui->show_uart) {
        orbital_logs_draw(ui->logs_uart, "UART Output", &ui->show_uart);
    }
    orbital_procs_draw(ui->procs, "Processes", true);
}

static void* orbital_display_main(void* arg)
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
    *wd = ImGui_ImplVulkanH_WindowData_Create();
    SetupVulkanWindowData(wd, vks, w, h);

    // Setup Dear ImGui binding
    igCreateContext(NULL);
    ImGuiIO* io = igGetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

    // Setup SDL binding
    ImGui_ImplSDL2_InitForVulkan(ui.sdl_window);

    // Setup Vulkan binding
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = vks->instance;
    init_info.PhysicalDevice = vks->gpu;
    init_info.Device = vks->device;
    init_info.QueueFamily = vks->graphics_queue_node_index;
    init_info.Queue = vks->queue;
    init_info.PipelineCache = NULL;
    init_info.DescriptorPool = vks->descriptor_pool; // todo: check this
    init_info.Allocator = NULL;
    init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info, wd->RenderPass);

    // Setup style
    igStyleColorsDark(NULL);

     // Upload Fonts
    {
        // Use any command queue
        VkCommandPool command_pool = wd->Frames[wd->FrameIndex].CommandPool;
        VkCommandBuffer command_buffer = wd->Frames[wd->FrameIndex].CommandBuffer;

        err = vkResetCommandPool(vks->device, command_pool, 0);
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
        err = vkQueueSubmit(vks->queue, 1, &end_info, VK_NULL_HANDLE);
        check_vk_result(err);

        err = vkDeviceWaitIdle(vks->device);
        check_vk_result(err);
        ImGui_ImplVulkan_InvalidateFontUploadObjects();
    }

    // Initialization
    float clear_color[4] = {0.45f, 0.55f, 0.60f, 1.00f};
    memcpy(&wd->ClearValue.color.float32[0], &clear_color, 4 * sizeof(float));
    ui.logs_uart = orbital_logs_create();
    ui.stats = orbital_stats_create();
    ui.procs = orbital_procs_create();
    ui.show_stats = true;
    ui.show_uart = false;
    ui.show_trace_cp = false;
    ui.show_trace_icc = false;
    ui.show_trace_samu = false;
    ui.show_mem_gpa = false;
    ui.show_mem_gva = false;
    ui.show_mem_gart = false;
    ui.show_mem_iommu = false;
    assert(ui.logs_uart);
    assert(ui.stats);
    assert(ui.procs);
    ui.active = true;

    quit = false;
    while (!quit) {
        SDL_Event event;
        // Events
        while (SDL_PollEvent(&evt) == 1) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (evt.type == SDL_QUIT) {
                quit = true;
            }
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_RESIZED &&
                event.window.windowID == SDL_GetWindowID(ui.sdl_window)) {
                ImGui_ImplVulkanH_CreateWindowDataSwapChainAndFramebuffer(
                    vks->gpu, vks->device, wd, NULL,
                    (int)event.window.data1, (int)event.window.data2);
            }
        }
        // Frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame(ui.sdl_window);
        igNewFrame();

        // Window
        orbital_display_draw(&ui);

        // Rendering
        igRender();
        FrameRender(wd, vks);
        FramePresent(wd, vks);
    }

    err = vkDeviceWaitIdle(vks->device);
    check_vk_result(err);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    igDestroyContext(NULL);
    SDL_DestroyWindow(ui.sdl_window);
    CleanupVulkan(wd, vks);
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
