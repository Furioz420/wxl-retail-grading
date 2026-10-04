// Copyright (C) 2026 WarcraftXL. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Grading.hpp"
#include <d3d9.h>
#include <wrl/client.h>
#include <cmath>

namespace wxl::grading
{
    inline bool Valid(const GradingTuning& t)
    {
        return std::isfinite(t.strength) && t.strength>=0 && t.strength<=1 &&
            std::isfinite(t.brightness) && t.brightness>=.75f && t.brightness<=1.25f &&
            std::isfinite(t.contrast) && t.contrast>=.5f && t.contrast<=1.8f &&
            std::isfinite(t.gamma) && t.gamma>=.5f && t.gamma<=1.8f &&
            std::isfinite(t.sharpness) && t.sharpness>=0 && t.sharpness<=3 && std::isfinite(t.brightnessOffset) && std::abs(t.brightnessOffset)<=.5f;
    }
    inline bool Neutral(const GradingTuning& t)
    { return t.brightnessOffset==0 && t.brightness==1 && t.contrast==1 && t.gamma==1 && t.sharpness==0; }
    inline bool Supported(IDirect3DDevice9* d)
    {
        using Microsoft::WRL::ComPtr;
        if (!d) return false;
        D3DVIEWPORT9 vp{}; D3DSURFACE_DESC desc{};
        ComPtr<IDirect3DSurface9> target,back;
        if (FAILED(d->GetRenderTarget(0,target.GetAddressOf())) ||
            FAILED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,back.GetAddressOf())) || target.Get()!=back.Get() ||
            FAILED(target->GetDesc(&desc)) || FAILED(d->GetViewport(&vp)) ||
            vp.X || vp.Y || vp.Width!=desc.Width || vp.Height!=desc.Height ||
            desc.MultiSampleType!=D3DMULTISAMPLE_NONE) return false;
        for (DWORD i=1;i<4;++i)
        {
            ComPtr<IDirect3DSurface9> extra;
            const HRESULT hr=d->GetRenderTarget(i,extra.GetAddressOf());
            if (extra || (FAILED(hr) && hr!=D3DERR_NOTFOUND && hr!=D3DERR_INVALIDCALL)) return false;
        }
        return true;
    }
    inline HRESULT Draw(IDirect3DDevice9* d,IDirect3DPixelShader9* shader,
                        IDirect3DTexture9* scene,IDirect3DTexture9* lut,const GradingTuning& tuning,
        IDirect3DTexture9* curve=nullptr,float curveStrength=0)
    {
        using Microsoft::WRL::ComPtr;
        if (!d || !shader || !scene || !Valid(tuning) || !std::isfinite(curveStrength) || curveStrength<0 || curveStrength>1) return D3DERR_INVALIDCALL;
        if (!Supported(d)) return S_FALSE;
        D3DVIEWPORT9 vp{}; D3DSURFACE_DESC desc{};
        if (FAILED(d->GetViewport(&vp)) || FAILED(scene->GetLevelDesc(0,&desc)) ||
            desc.Width!=vp.Width || desc.Height!=vp.Height) return S_FALSE;
        // Refuse an input that aliases the active target.
        ComPtr<IDirect3DSurface9> input,target;
        if (FAILED(scene->GetSurfaceLevel(0,input.GetAddressOf())) ||
            FAILED(d->GetRenderTarget(0,target.GetAddressOf())) || input.Get()==target.Get()) return S_FALSE;
        ComPtr<IDirect3DStateBlock9> saved;
        HRESULT hr=d->CreateStateBlock(D3DSBT_ALL,saved.GetAddressOf());
        if (FAILED(hr) || FAILED(hr=saved->Capture())) return hr;
        auto set=[&](HRESULT result) { if (FAILED(result) && SUCCEEDED(hr)) hr=result; };
        set(d->SetVertexShader(nullptr)); set(d->SetPixelShader(shader));
        for (auto state : {D3DRS_ZENABLE,D3DRS_ZWRITEENABLE,D3DRS_STENCILENABLE,D3DRS_SCISSORTESTENABLE,
             D3DRS_ALPHATESTENABLE,D3DRS_FOGENABLE,D3DRS_SRGBWRITEENABLE,D3DRS_ALPHABLENDENABLE})
            set(d->SetRenderState(state,FALSE));
        set(d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE));
        set(d->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID));
        set(d->SetRenderState(D3DRS_COLORWRITEENABLE,7)); // Preserve destination alpha.
        set(d->SetTexture(2,curve ? curve : scene));
        set(d->SetTexture(0,scene)); set(d->SetTexture(1,lut ? lut : scene));
        for (DWORD sampler=0;sampler<3;++sampler)
        {
            set(d->SetSamplerState(sampler,D3DSAMP_MINFILTER,sampler==1 ? D3DTEXF_LINEAR : D3DTEXF_POINT));
            set(d->SetSamplerState(sampler,D3DSAMP_MAGFILTER,sampler==1 ? D3DTEXF_LINEAR : D3DTEXF_POINT));
            set(d->SetSamplerState(sampler,D3DSAMP_MIPFILTER,D3DTEXF_NONE));
            set(d->SetSamplerState(sampler,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP));
            set(d->SetSamplerState(sampler,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP));
            set(d->SetSamplerState(sampler,D3DSAMP_SRGBTEXTURE,FALSE));
        }
        const float constants[3][4]{{lut && !(curve && curveStrength>0) ? tuning.strength : 0,1.f/vp.Width,1.f/vp.Height,tuning.sharpness},
            {tuning.brightness,tuning.contrast,1/tuning.gamma,tuning.brightnessOffset},{curve ? curveStrength : 0,0,0,0}};
        set(d->SetPixelShaderConstantF(0,constants[0],3));
        struct V { float x,y,z,w,u,v; };
        const float width=static_cast<float>(vp.Width),height=static_cast<float>(vp.Height);
        const V quad[]{{-.5f,-.5f,0,1,0,0},{width-.5f,-.5f,0,1,1,0},
            {-.5f,height-.5f,0,1,0,1},{width-.5f,height-.5f,0,1,1,1}};
        set(d->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1));
        if (SUCCEEDED(hr)) hr=d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,quad,sizeof(V));
        const HRESULT restored=saved->Apply();
        return FAILED(restored) ? restored : hr;
    }
}
