/*
 * QEMU-Orbital user interface
 *
 * Copyright (c) 2017-2019 Alexandro Sanchez Bach
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

#include "orbital-debug-gpu.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define IMGUI_IMPL_API
#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl.h"
#include "imgui/imgui_impl_vulkan.h"

#include "hw/ps4/liverpool/dce/dce_8_0_d.h"
#include <array>

struct dcp_attribute_t {
    const char* name;
    int mmio_indices[6];
};

#define DCP_COUNT 6
#define DCP_ATTR(x) \
    { #x, { mmDCP0_##x, mmDCP1_##x, mmDCP2_##x, \
            mmDCP3_##x, mmDCP4_##x, mmDCP5_##x }}

std::array<dcp_attribute_t, 6> dcp_attrs = {{
    DCP_ATTR(GRPH_SURFACE_OFFSET_X),
    DCP_ATTR(GRPH_SURFACE_OFFSET_Y),
    DCP_ATTR(GRPH_X_START),
    DCP_ATTR(GRPH_Y_START),
    DCP_ATTR(GRPH_X_END),
    DCP_ATTR(GRPH_Y_END),
}};

struct orbital_debug_gpu_t
{
    uint32_t *mmio;

    void Draw_DCE()
    {
        if (!mmio)
            return;

        if (ImGui::CollapsingHeader("DCP")) {
            ImGui::Columns(DCP_COUNT + 1, "DCP_Columns");
            ImGui::Separator();
            ImGui::Text("Attribute");
            ImGui::NextColumn();
            for (int i = 0; i < DCP_COUNT; i++) {
                ImGui::Text("DCP%d", i);
                ImGui::NextColumn();
            }
            ImGui::Separator();
            for (const auto& attr : dcp_attrs) {
                ImGui::Text("%s", attr.name);
                ImGui::NextColumn();
                for (int i = 0; i < DCP_COUNT; i++) {
                    int mm_index = attr.mmio_indices[i];
                    ImGui::Text("%d", mmio[mm_index]);
                    ImGui::NextColumn();
                }
            }
        }
    }

    void Draw_GFX()
    {
    }

    void Draw_SAM()
    {
    }

    void Draw(const char* title, bool* p_open = NULL)
    {
        ImGui::SetNextWindowSize(ImVec2(500,400), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(title, p_open)) {
            ImGui::End();
            return;
        }
        ImGuiTabBarFlags tab_flags = ImGuiTabBarFlags_None;
        if (ImGui::BeginTabBar("Engines", tab_flags)) {
            if (ImGui::BeginTabItem("DCE")) {
                Draw_DCE();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("GFX")) {
                Draw_GFX();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("SAM")) {
                Draw_SAM();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End();
    }
};

extern "C" {

struct orbital_debug_gpu_t* orbital_debug_gpu_create(void)
{
    struct orbital_debug_gpu_t *widget;
    
    widget = new orbital_debug_gpu_t();
    widget->mmio = nullptr;
    return widget;
}

void orbital_debug_gpu_destroy(struct orbital_debug_gpu_t *widget)
{
    delete widget;
}

void orbital_debug_gpu_draw(struct orbital_debug_gpu_t *widget, const char *title, bool* p_open)
{
    widget->Draw(title, p_open);
}

void orbital_debug_gpu_set_mmio(struct orbital_debug_gpu_t *widget, uint32_t *mmio)
{
    widget->mmio = mmio;
}

} // extern "C"
