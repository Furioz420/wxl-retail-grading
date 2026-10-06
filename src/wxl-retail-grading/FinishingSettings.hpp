// Copyright (C) 2026 WarcraftXL. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Grading.hpp"
#include "RenderSettings.hpp"
#include <algorithm>
namespace wxl::grading::preferences {
    namespace settings=wxl::render::settings;
    inline constexpr settings::Field fields[]{ {"enabled",0,1,true},{"brightness",.75f,1.25f},
        {"contrast",.5f,1.8f},{"gamma",.5f,1.8f},{"sharpness",0,3},{"brightnessOffset",-.5f,.5f} };
    inline constexpr settings::Field legacy[]{ {"enabled",0,1,true},{"brightness",.75f,1.25f},
        {"contrast",.75f,1.25f},{"gamma",.75f,1.5f},{"sharpness",0,.6f} };
    inline settings::Result Load(const std::filesystem::path& path,GradingTuning& t) {
        std::array<float,6> values{}; auto result=settings::Load(path,fields,values);
        if(result==settings::Result::Invalid) {
            std::array<float,5> old{}; result=settings::Load(path,legacy,old);
            if(result==settings::Result::Ok) std::copy(old.begin(),old.end(),values.begin());
        }
        if(result==settings::Result::Ok) {
            t.enabled=values[0]!=0; t.brightness=values[1]; t.contrast=values[2];
            t.gamma=values[3]; t.sharpness=values[4]; t.brightnessOffset=values[5];
        }
        return result;
    }
    inline settings::Result Save(const std::filesystem::path& path,const GradingTuning& t) {
        return settings::Save(path,fields,std::array<float,6>{t.enabled?1.f:0.f,t.brightness,t.contrast,t.gamma,t.sharpness,t.brightnessOffset});
    }
    inline settings::Result LoadPreferred(const std::filesystem::path& current,const std::filesystem::path& previous,GradingTuning& t) {
        const auto result=Load(current,t);
        return result==settings::Result::Missing ? Load(previous,t) : result;
    }
}
