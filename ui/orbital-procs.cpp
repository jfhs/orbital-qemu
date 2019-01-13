/*
 * QEMU-Orbital user interface
 *
 * Copyright (c) 2017-2018 Alexandro Sanchez Bach
 * Copyright (c) 2017-2018 jfhs
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

#include "orbital-procs.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define IMGUI_IMPL_API
#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl.h"
#include "imgui/imgui_impl_vulkan.h"

extern int smp_cpus;

struct orbital_procs_t
{
    uint32_t cpu_count;
    orbital_procs_cpu_data* cpus;

    orbital_procs_t() {
        cpu_count = smp_cpus;
        cpus = (orbital_procs_cpu_data*)calloc(cpu_count, sizeof(orbital_procs_cpu_data));
    }

    ~orbital_procs_t() {
        free(cpus);
    }

    void Update(uint32_t cpuid, orbital_procs_cpu_data data)
    {
        if (cpuid > cpu_count) {
            printf("Got cpuid (%d) > than cpu_count (%d)", cpuid, cpu_count);
            return;
        }
        if (!strcmp(data.proc_name, "idle")) {
            cpus[cpuid].idle_counter++;
        } else {
            cpus[cpuid] = data;
            cpus[cpuid].idle_counter = 0;
        }
    }

    void Draw(const char* title, bool* p_open = NULL)
    {
        ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(title, p_open)) {
            ImGui::End();
            return;
        }

        for (auto i = 0; i < cpu_count; ++i) {
            ImGui::Text("CPU %d: %s(%d) IDLES=%llx GS=%llx TPTR=%llx PROCPTR=%llx", i, cpus[i].proc_name, (int)cpus[i].pid, cpus[i].idle_counter, cpus[i].gs, cpus[i].thread_pointer, cpus[i].proc_pointer);
        }

        ImGui::End();
    }
};

extern "C" {

struct orbital_procs_t* orbital_procs_create(void)
{
    struct orbital_procs_t *procs;

    procs = new orbital_procs_t();
    procs->cpu_count = smp_cpus;
    procs->cpus = new orbital_procs_cpu_data[smp_cpus];
    return procs;
}

void orbital_procs_destroy(struct orbital_procs_t *procs)
{
    delete procs;
}

void orbital_procs_draw(struct orbital_procs_t *procs, const char *title, bool* p_open)
{
    procs->Draw(title, p_open);
}

void orbital_procs_update(struct orbital_procs_t *procs, uint32_t cpuid, orbital_procs_cpu_data data)
{
    procs->Update(cpuid, data);
}

} // extern "C"
