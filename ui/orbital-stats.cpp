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

#include "orbital-stats.h"
#include "orbital.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define IMGUI_IMPL_API
#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl.h"
#include "imgui/imgui_impl_vulkan.h"

#include <algorithm>
#include <unordered_map>

typedef struct orbital_stats_usage_t {
    bool used;
    double last_r;
    double last_w;
    double last;
} orbital_stats_usage_t;

typedef struct orbital_stats_device_info_t {
    int id;
    const char* devid;
    const char* name;
} orbital_stats_device_t;

typedef struct orbital_stats_device_usage_t {
    orbital_stats_usage_t pci;
    orbital_stats_usage_t bar[6];
    orbital_stats_usage_t msi;
} orbital_stats_device_usage_t;

static const orbital_stats_device_info_t devices[] = {
    // Aeolia
    { UI_DEVICE_AEOLIA_ACPI,    "104D:908F", "Aeolia ACPI" },
    { UI_DEVICE_AEOLIA_GBE,     "104D:909E", "Aeolia GBE" },
    { UI_DEVICE_AEOLIA_AHCI,    "104D:909F", "Aeolia AHCI" },
    { UI_DEVICE_AEOLIA_SDHCI,   "104D:90A0", "Aeolia SDHCI" },
    { UI_DEVICE_AEOLIA_PCIE,    "104D:90A1", "Aeolia PCIE" },
    { UI_DEVICE_AEOLIA_DMAC,    "104D:90A2", "Aeolia DMAC" },
    { UI_DEVICE_AEOLIA_DDR3,    "104D:90A3", "Aeolia SPM" },
    { UI_DEVICE_AEOLIA_XHCI,    "104D:90A4", "Aeolia XHCI" },
    // Liverpool
    { UI_DEVICE_LIVERPOOL_GC,   "1002:9920", "Liverpool GC" },
    { UI_DEVICE_LIVERPOOL_HDAC, "1002:9921", "Liverpool HDAC" },
};

struct orbital_stats_t
{
    std::unordered_map<int, orbital_stats_device_usage_t> dev_usages;

    static void DrawUsageBox(const orbital_stats_usage_t& usage)
    {
        float hue = 0.6f;
        float sv = !usage.used ? 0.2 : 0.4 +
            0.5 * std::max(0.0, 1.0 - (ImGui::GetTime() - usage.last));
        ImGui::PushStyleColor(ImGuiCol_Button,
            (ImVec4)ImColor::HSV(hue, sv / 2.0, sv));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            (ImVec4)ImColor::HSV(hue, sv / 2.0, sv));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
            (ImVec4)ImColor::HSV(hue, sv / 2.0, sv));
        ImGui::Button("RW");
        ImGui::PopStyleColor(3);
    }

    void Draw(const char* title, bool* p_open = NULL)
    {
        ImGui::SetNextWindowSize(ImVec2(500,400), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(title, p_open)) {
            ImGui::End();
            return;
        }
        ImGui::Text("Average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        if (ImGui::CollapsingHeader("Hardware")) {
            ImGui::Columns(4, "mycolumns");
            ImGui::Separator();
            ImGui::PushItemWidth(200);
            ImGui::Text("ID");   ImGui::NextColumn();
            ImGui::PopItemWidth();
            ImGui::Text("Name"); ImGui::NextColumn();
            ImGui::Text("PCI");  ImGui::NextColumn();
            ImGui::Text("BARs");  ImGui::NextColumn();
            ImGui::Separator();
            for (int i = 0; i < IM_ARRAYSIZE(devices); i++) {
                ImGui::PushItemWidth(200);
                ImGui::Text("%s", devices[i].devid);
                ImGui::NextColumn();
                ImGui::PopItemWidth();
                ImGui::Text("%s", devices[i].name);
                ImGui::NextColumn();
                DrawUsageBox(dev_usages[devices[i].id].pci);
                ImGui::NextColumn();
                for (int j = 0; j < 6; j++) {
                    if (j > 0)
                        ImGui::SameLine();
                    DrawUsageBox(dev_usages[devices[i].id].bar[j]);
                }
                ImGui::NextColumn();
            }
            ImGui::Columns(1);
            ImGui::Separator();
        }
        ImGui::End();
    }

    void Log(int device, int component, int event)
    {
        orbital_stats_device_usage_t& dev_usage = dev_usages[device];
        orbital_stats_usage_t* usage;

        switch (component) {
        case UI_DEVICE_BAR0:  usage = &dev_usage.bar[0];  break;
        case UI_DEVICE_BAR1:  usage = &dev_usage.bar[1];  break;
        case UI_DEVICE_BAR2:  usage = &dev_usage.bar[2];  break;
        case UI_DEVICE_BAR3:  usage = &dev_usage.bar[3];  break;
        case UI_DEVICE_BAR4:  usage = &dev_usage.bar[4];  break;
        case UI_DEVICE_BAR5:  usage = &dev_usage.bar[5];  break;
        case UI_DEVICE_MSI:   usage = &dev_usage.msi;     break;
        default:
            return;
        }

        usage->used = true;
        usage->last = ImGui::GetTime();
        switch (event) {
        case UI_DEVICE_READ:
            usage->last_r = usage->last;
            break;
        case UI_DEVICE_WRITE:
            usage->last_w = usage->last;
            break;
        }
    }
};

extern "C" {

struct orbital_stats_t* orbital_stats_create(void)
{
    struct orbital_stats_t *stats;
    
    stats = new orbital_stats_t();
    return stats;
}

void orbital_stats_destroy(struct orbital_stats_t *stats)
{
    delete stats;
}

void orbital_stats_draw(struct orbital_stats_t *stats, const char *title, bool* p_open)
{
    stats->Draw(title, p_open);
}

void orbital_stats_log(struct orbital_stats_t *stats, int device, int component, int event)
{
    stats->Log(device, component, event);
}

} // extern "C"
