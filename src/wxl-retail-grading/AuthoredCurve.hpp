// Copyright (C) 2026 WarcraftXL. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "wxl/AuthoredGradingFrame.hpp"
#include <d3d9.h>
#include <wrl/client.h>
namespace wxl::grading {
inline bool UploadCurve(IDirect3DDevice9* d,const WXL_AuthoredGrading& v,Microsoft::WRL::ComPtr<IDirect3DTexture9>& texture) {
    if(!d || !ValidCurve(v)) return false;
    if(!texture && FAILED(d->CreateTexture(32,1,1,0,D3DFMT_A32B32G32R32F,D3DPOOL_MANAGED,texture.GetAddressOf(),nullptr))) return false;
    D3DLOCKED_RECT lock{};
    if(FAILED(texture->LockRect(0,&lock,nullptr,0))) return false;
    auto* values=static_cast<float*>(lock.pBits);
    for(int i=0;i<32;++i) for(int channel=0;channel<4;++channel) values[i*4+channel]=v.curve[i];
    return SUCCEEDED(texture->UnlockRect(0));
}
}
