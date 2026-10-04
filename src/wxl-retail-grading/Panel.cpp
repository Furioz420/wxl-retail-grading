// The mood picker: which cube, how hard.
// Copyright (C) 2026 WarcraftXL
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "wxl-retail-grading/Grading.hpp"
#include "wxl-retail-grading/FinishingProfiles.hpp"

#include "config.hpp"
#include "engine/hook/Registry.hpp"
#include "engine/ui/ImGuiHost.hpp"

#include "imgui.h"

namespace
{
    void Draw(void*)
    {
        wxl::grading::GradingTuning& g = wxl::grading::Tuning();
        const std::vector<std::string>& luts = wxl::grading::Luts();

        ImGui::Checkbox("Enabled", &g.enabled);
        ImGui::TextUnformatted("Finishing presets");
        const char* names[]{"Balanced","Natural","Vivid","Cinematic","Finishing off"};
        for(int i=0;i<5;++i) {
            if(i==1 || i==2 || i==4) ImGui::SameLine();
            if(ImGui::Button(names[i])) wxl::grading::ApplyProfile(g,static_cast<wxl::grading::Profile>(i));
        }
        const char* selected="Custom / startup settings";
        for(int i=0;i<5;++i) {
            auto preset=g; wxl::grading::ApplyProfile(preset,static_cast<wxl::grading::Profile>(i));
            if(g.enabled==preset.enabled && g.brightness==preset.brightness && g.brightnessOffset==preset.brightnessOffset &&
                g.contrast==preset.contrast && g.gamma==preset.gamma && g.sharpness==preset.sharpness) selected=names[i];
        }
        ImGui::Text("Current: %s",selected);
        if (ImGui::Button("Previous WXL look")) { g.brightnessOffset=0; g.brightness=1; g.contrast=1.02f; g.gamma=1; g.sharpness=.15f; }
        ImGui::SameLine();
        if (ImGui::Button("Neutral")) { g.brightnessOffset=0; g.brightness=g.contrast=g.gamma=1; g.sharpness=0; }
        ImGui::SliderFloat("Brightness offset",&g.brightnessOffset,-.5f,.5f,"%.2f");
        ImGui::SliderFloat("Brightness multiplier",&g.brightness,.75f,1.25f,"%.2fx");
        ImGui::SliderFloat("Contrast",&g.contrast,.5f,1.8f,"%.2fx");
        ImGui::SliderFloat("Gamma",&g.gamma,.5f,1.8f,"%.2f");
        ImGui::SliderFloat("Sharpness",&g.sharpness,0,3,"%.2f");
        if(ImGui::Button("Save")) wxl::grading::SaveFinishing();
        ImGui::SameLine();
        if(ImGui::Button("Reload")) wxl::grading::ReloadFinishing();
        if(ImGui::Button("Restore startup")) wxl::grading::RestoreFinishing();
        ImGui::TextWrapped("%s",wxl::grading::SettingsNote());
        ImGui::TextWrapped("World only; UI stays unchanged. Save keeps these finishing controls for next time. Colour-cube selection and strength remain session-only.");
        if (!ImGui::CollapsingHeader("Advanced colour cube")) return;

        if (luts.empty())
        {
            ImGui::TextDisabled("No cubes found under Data\\*\\environments\\colorgrading\\.");
        }
        else
        {
            const int cur = wxl::grading::CurrentLut();
            const char* label = (cur >= 0) ? luts[cur].c_str() : "(none)";
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##lut", label))
            {
                for (int i = 0; i < static_cast<int>(luts.size()); ++i)
                    if (ImGui::Selectable(luts[i].c_str(), i == cur))
                        wxl::grading::SelectLut(i);
                ImGui::EndCombo();
            }
        }

        ImGui::SliderFloat("Strength", &g.strength, 0.0f, 1.0f, "%.2f");
        if (ImGui::Button("Rescan folder"))
            wxl::grading::Rescan();
    }

    bool Install()
    {
        wxl::ui::AddPanel("Grading", &Draw, nullptr, 360.0f, 0.0f);
        return true;
    }
}

WXL_REGISTER_FEATURE("retail-grading-panel", wxl::features::retailGrading, Install)
