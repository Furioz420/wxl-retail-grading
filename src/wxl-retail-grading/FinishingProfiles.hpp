// Copyright (C) 2026 WarcraftXL. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Grading.hpp"
namespace wxl::grading {
    // Numeric reference: modern-wow-renderer 0c7717d8 presets. WXL retains its
    // local-range sharpening clamp. Its normalized Laplacian requires 3x the gain.
    enum class Profile { Balanced,Natural,Vivid,Cinematic,Performance };
    inline void ApplyProfile(GradingTuning& t,Profile profile) {
        t.enabled=profile!=Profile::Performance; t.brightness=1; t.brightnessOffset=0;
        t.contrast=1; t.gamma=1; t.sharpness=1.05f;
        switch(profile) {
        case Profile::Natural: t.contrast=.98f; t.sharpness=.75f; break;
        case Profile::Vivid: t.contrast=1.18f; t.gamma=.97f; t.sharpness=1.5f; break;
        case Profile::Cinematic: t.contrast=1.08f; t.gamma=1.02f; t.sharpness=1.35f; break;
        case Profile::Performance: t.sharpness=0; break;
        default: break;
        }
    }
}
