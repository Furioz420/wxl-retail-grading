// The zone's mood as a colour cube: what the grading pass is told to do.
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

#pragma once

#include <string>
#include <vector>

namespace wxl::grading
{
    struct GradingTuning
    {
        /// World finishing/LUT master switch. No LUT is required for finishing controls.
        bool enabled = true;

        /// How much of the graded colour is used. One is the cube as authored; between is a
        /// dial every authored mood can be walked in on.
        float strength = 1.0f;
        // Explicitly saved finishing controls. Independent of the optional LUT.
        float brightness = 1.0f;
        float contrast = 1.02f;
        float gamma = 1.0f;
        float sharpness = 0.15f;
        float brightnessOffset = 0.0f;
    };

    GradingTuning& Tuning();
    void SaveFinishing();
    void ReloadFinishing();
    void RestoreFinishing();
    const char* SettingsNote();

    /// The cubes found on disk, for the panel. Bare file names; index into SelectLut.
    const std::vector<std::string>& Luts();

    /// Currently selected cube's index in Luts(), or -1 when none is loaded.
    int CurrentLut();

    /// Picks a cube by index; the load happens on the next frame's pass.
    void SelectLut(int index);

    /// Re-scans the colorgrading folders. The selection is kept by name when it survives.
    void Rescan();
}
