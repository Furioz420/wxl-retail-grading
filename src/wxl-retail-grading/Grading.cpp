// Colour grading: the frame remapped through an authored cube at the world -> UI boundary.
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
//
// THE MOOD IS A TEXTURE. The modern client runs every frame through a 32x32x32 colour cube chosen
// per zone and per hour -- an artist's grade of the lit scene, not a lighting change. The cubes
// ship as ordinary 1024x32 strips, this pass is two taps and a lerp, and the frame it grades is
// the WORLD alone: it runs at the world -> UI boundary, so the interface stays un-graded exactly
// as it does there.
//
// The cubes are read straight off the loose patch tree with our own twenty-line BLP reader,
// rather than through the engine's texture path: a MANAGED texture survives device resets on its
// own, and a reader with no engine coupling costs nothing anybody has to maintain.

#include "wxl-retail-grading/Grading.hpp"

#include "config.hpp"
#include "runtime/Extensions.hpp"
#include "wxl/AuthoredGradingFrame.hpp"
#include "wxl/RenderReloadKey.hpp"
#include "common/Log.hpp"
#include "engine/hook/Registry.hpp"
#include "engine/events/Event.hpp"
#include "game/Gx.hpp"
#include "game/Render.hpp"

#include <windows.h>
#include <d3d9.h>
#include <cstdint>
#include <cstring>
#include <vector>

#include "wxl-retail-grading/shaders/GradingPs.h"
#include "wxl-retail-grading/GradingPass.hpp"
#include "wxl-retail-grading/AuthoredCurve.hpp"
#include "RenderSettings.hpp"
#include "FinishingSettings.hpp"

namespace
{
    namespace gx = wxl::game::gx;

    wxl::grading::GradingTuning g_tuning,g_startup;
    namespace settings=wxl::render::settings;
    constexpr wchar_t kSettingsFile[]=L"wxl-finishing-v2.ini";
    const char* g_note="";
    settings::Result LoadFinishing()
    {
        const auto result=wxl::grading::preferences::LoadPreferred(kSettingsFile,L"wxl-finishing.ini",g_tuning);
        WLOG_INFO("grading: load finishing result=%u",static_cast<unsigned>(result));
        return result;
    }

    std::vector<std::string> g_names;   ///< bare file names, panel-facing
    std::vector<std::string> g_paths;   ///< full paths, loader-facing
    int  g_current = -1;
    int  g_wanted  = -1;                ///< selection the next pass should load
    bool g_scanned = false;

    IDirect3DTexture9*    g_lut = nullptr;    ///< MANAGED: survives device resets by itself
    IDirect3DPixelShader9* g_ps = nullptr;    ///< device object: dropped on reset, remade lazily
    gx::RenderTarget       g_frame;           ///< the world frame, captured to be sampled
    Microsoft::WRL::ComPtr<IDirect3DTexture9> g_curve;
    WXL_AuthoredGrading g_authored{};
    unsigned               g_authoredReports = 0;
    unsigned               g_logged = 0;
    unsigned               g_drawReports = 0;

    void EnsureDevice(IDirect3DDevice9* device)
    {
        // Keep identity alive until replacement is observed. MANAGED textures
        // survive Reset, but cannot be used on a different D3D9 device.
        static auto* owner = new Microsoft::WRL::ComPtr<IDirect3DDevice9>;
        if (owner->Get() == device) return;
        if (*owner)
        {
            g_curve.Reset();
            gx::Release(g_frame);
            if (g_ps) { g_ps->Release(); g_ps = nullptr; }
            if (g_lut) { g_lut->Release(); g_lut = nullptr; }
            // Preserve a pending user selection, otherwise reload the active LUT.
            if (g_wanted < 0) g_wanted = g_current;
            g_current = -1;
            g_logged = g_drawReports = 0;
            WLOG_INFO("grading: replaced device; rebuilding resources with current preferences");
        }
        *owner = device;
    }

    /// Everything under Data/<any folder>/environments/colorgrading. The patch is a loose tree in
    /// this deployment, which is what makes a plain directory scan the honest source of truth.
    void Scan()
    {
        g_names.clear();
        g_paths.clear();
        WIN32_FIND_DATAA top;
        HANDLE ht = FindFirstFileA("Data\\*", &top);
        if (ht == INVALID_HANDLE_VALUE) return;
        do
        {
            if (!(top.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            if (top.cFileName[0] == '.') continue;
            char pat[MAX_PATH];
            std::snprintf(pat, sizeof(pat), "Data\\%s\\environments\\colorgrading\\*.blp",
                          top.cFileName);
            WIN32_FIND_DATAA fd;
            HANDLE h = FindFirstFileA(pat, &fd);
            if (h == INVALID_HANDLE_VALUE) continue;
            do
            {
                char full[MAX_PATH];
                std::snprintf(full, sizeof(full), "Data\\%s\\environments\\colorgrading\\%s",
                              top.cFileName, fd.cFileName);
                g_names.emplace_back(fd.cFileName);
                g_paths.emplace_back(full);
            } while (FindNextFileA(h, &fd));
            FindClose(h);
        } while (FindNextFileA(ht, &top));
        FindClose(ht);
        g_scanned = true;
        WLOG_INFO("grading: %u cube(s) found", static_cast<unsigned>(g_names.size()));
    }

    /// Uncompressed BLP2 strip -> MANAGED texture. Anything else is refused with its reason: a
    /// cube that silently half-loads is a mood nobody can debug.
    bool LoadLut(IDirect3DDevice9* d, int index)
    {
        if (index < 0 || index >= static_cast<int>(g_paths.size())) return false;
        HANDLE f = CreateFileA(g_paths[index].c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                               OPEN_EXISTING, 0, nullptr);
        if (f == INVALID_HANDLE_VALUE)
        {
            WLOG_WARN("grading: cannot open '%s'", g_paths[index].c_str());
            return false;
        }
        DWORD size = GetFileSize(f, nullptr), got = 0;
        std::vector<uint8_t> blp(size);
        ReadFile(f, blp.data(), size, &got, nullptr);
        CloseHandle(f);
        if (got < 1172 || std::memcmp(blp.data(), "BLP2", 4) != 0 || blp[8] != 3)
        {
            WLOG_WARN("grading: '%s' is not an uncompressed BLP2 -- refused", g_names[index].c_str());
            return false;
        }
        uint32_t w, h, off;
        std::memcpy(&w, blp.data() + 12, 4);
        std::memcpy(&h, blp.data() + 16, 4);
        std::memcpy(&off, blp.data() + 20, 4);
        if (w != h * 32 || static_cast<uint64_t>(off) + static_cast<uint64_t>(w) * h * 4 > size)
        {
            WLOG_WARN("grading: '%s' is %ux%u, not a 32-slice strip -- refused",
                      g_names[index].c_str(), w, h);
            return false;
        }

        IDirect3DTexture9* tex = nullptr;
        if (FAILED(d->CreateTexture(w, h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, nullptr)))
            return false;
        D3DLOCKED_RECT lr;
        if (FAILED(tex->LockRect(0, &lr, nullptr, 0)))
        {
            tex->Release();
            return false;
        }
        for (uint32_t row = 0; row < h; ++row)
            std::memcpy(static_cast<uint8_t*>(lr.pBits) + row * lr.Pitch,
                        blp.data() + off + static_cast<size_t>(row) * w * 4, w * 4);
        tex->UnlockRect(0);

        if (g_lut) g_lut->Release();
        g_lut = tex;
        g_current = index;
        WLOG_INFO("grading: cube '%s' loaded", g_names[index].c_str());
        return true;
    }

    /// The pass, at the frame's own post-fx slot: capture what the world drew, then draw it back
    /// through the cube. Every piece of device state touched is read first and put back after --
    /// the UI renders next. A complete state block restores constants, streams and sampler state.
    void OnWorldEnd(void*, const void* a)
    {
        if(!wxl::game::render::EffectsEnabled()) return;
        if (!g_tuning.enabled || !wxl::grading::Valid(g_tuning)) return;
        auto* d = static_cast<IDirect3DDevice9*>(
            const_cast<void*>(static_cast<const wxl::events::WorldRenderEndArgs*>(a)->device));
        if (!d) return;
        EnsureDevice(d);

        if (g_wanted >= 0)
        {
            LoadLut(d, g_wanted);
            g_wanted = -1;
        }
        const bool authored=wxl::grading::UploadCurve(d,g_authored,g_curve);
        if (!authored && (!g_lut || g_tuning.strength==0) && wxl::grading::Neutral(g_tuning)) return;
        // The copy and full-screen pass share an exact backbuffer/viewport contract.
        if (!wxl::grading::Supported(d)) return;

        if (!g_ps && FAILED(d->CreatePixelShader(
                reinterpret_cast<const DWORD*>(kGradingPs), &g_ps)))
            return;

        gx::Device9 gdev(d);
        if (!gx::EnsureBackbufferTarget(gdev, g_frame, D3DFMT_A8R8G8B8)) return;

        IDirect3DSurface9* rt = nullptr;
        if (FAILED(d->GetRenderTarget(0, &rt)) || !rt) return;
        const HRESULT hr = d->StretchRect(rt, nullptr,
                                          static_cast<IDirect3DSurface9*>(g_frame.surface),
                                          nullptr, D3DTEXF_NONE);
        rt->Release();
        if (FAILED(hr))
        {
            if (g_logged < 4)
            {
                ++g_logged;
                WLOG_WARN("grading: frame copy failed hr=0x%08lX", static_cast<unsigned long>(hr));
            }
            return;
        }

        const HRESULT drawn=wxl::grading::Draw(d,g_ps,
            static_cast<IDirect3DTexture9*>(g_frame.texture),g_lut,g_tuning,authored ? g_curve.Get() : nullptr,authored ? g_authored.strength : 0);
        if(drawn==S_OK && authored && g_authoredReports++<3) WLOG_INFO("grading: Forever authored curve active strength=%.2f",g_authored.strength);
        if (drawn==S_OK && g_drawReports++<3)
            WLOG_INFO("grading: finishing active brightness=%.3f contrast=%.3f gamma=%.3f sharpness=%.3f lut=%u",
                g_tuning.brightness,g_tuning.contrast,g_tuning.gamma,g_tuning.sharpness,g_lut ? 1u : 0u);
        if (FAILED(drawn) && g_logged++<4)
            WLOG_WARN("grading: draw/state restoration failed hr=0x%08lX",static_cast<unsigned long>(drawn));
    }

    void OnDeviceLost(void*, const void*)
    {
        gx::Release(g_frame);
        if (g_ps) { g_ps->Release(); g_ps = nullptr; }
        // The MANAGED cube rides the reset untouched.
    }

    void __cdecl OnRenderPass(void* user, const WXL_RenderPassContext* pass)
    {
        g_authored={sizeof(g_authored)};
        const auto* api=static_cast<const WXL_AuthoredGradingApi*>(wxl::runtime::extensions::GetInterface("wxl.authored-grading",WXL_AUTHORED_GRADING_API_VERSION));
        if(api && api->structSize==sizeof(*api) && api->apiVersion==WXL_AUTHORED_GRADING_API_VERSION && api->GetCurve)
            api->GetCurve(pass->frameId,pass->deviceGeneration,&g_authored);
        const wxl::events::WorldRenderEndArgs args{pass->device};
        OnWorldEnd(user, &args);
    }

    void OnReloadInput(void*,const void* args) {
        const auto* a=static_cast<const wxl::events::InputArgs*>(args);
        static wxl::render::ReloadKey key;
        if(key.Handle(a->message,a->wparam,a->handled)) wxl::grading::ReloadFinishing();
    }
    bool Install()
    {
        wxl::events::Subscribe(wxl::events::Event::OnInput,OnReloadInput,nullptr);
        LoadFinishing(); g_startup=g_tuning;
        Scan();
        // The identity cube is the honest default: on with strength one, it changes nothing,
        // and every mood is one combo click away from a known-neutral baseline.
        for (size_t i = 0; i < g_names.size(); ++i)
            if (_stricmp(g_names[i].c_str(), "colorgradingidentity.blp") == 0)
                g_wanted = static_cast<int>(i);
        if (g_wanted < 0 && !g_names.empty()) g_wanted = 0;

        if (const auto* api = wxl::game::render::Api())
        {
            if (!api->AddPostProcess(WXL_RENDER_GRADING, OnRenderPass, nullptr)) return false;
        }
        else wxl::events::Subscribe(wxl::events::Event::OnWorldRenderEnd, &OnWorldEnd, nullptr);
        wxl::events::Subscribe(wxl::events::Event::OnDeviceLost, &OnDeviceLost, nullptr);
        WLOG_INFO("grading: installed at the world/UI boundary");
        return true;
    }
}

namespace wxl::grading
{
    GradingTuning& Tuning() { return g_tuning; }
    const char* SettingsNote() { return g_note; }
    void SaveFinishing()
    {
        const auto result=preferences::Save(kSettingsFile,g_tuning);
        g_note=result==settings::Result::Ok ? "Finishing saved for next time." : settings::Message(result);
        WLOG_INFO("grading: save finishing result=%u",static_cast<unsigned>(result));
    }
    void ReloadFinishing()
    {
        const auto result=LoadFinishing();
        g_note=result==settings::Result::Ok ? "Saved finishing loaded." : settings::Message(result);
    }
    void RestoreFinishing()
    {
        const float lutStrength=g_tuning.strength; g_tuning=g_startup; g_tuning.strength=lutStrength;
        g_note="Startup finishing restored; saved file unchanged.";
    }
    const std::vector<std::string>& Luts() { return g_names; }
    int  CurrentLut() { return g_current; }
    void SelectLut(int index) { g_wanted = index; }
    void Rescan()
    {
        const std::string keep = (g_current >= 0) ? g_names[g_current] : std::string();
        Scan();
        g_current = -1;
        for (size_t i = 0; i < g_names.size(); ++i)
            if (g_names[i] == keep) g_wanted = static_cast<int>(i);
    }
}

WXL_REGISTER_FEATURE("retail-grading", wxl::features::retailGrading, Install)
