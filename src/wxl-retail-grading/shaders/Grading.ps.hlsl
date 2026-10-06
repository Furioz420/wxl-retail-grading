// Colour grading: the frame remapped through a zone's authored look-up cube.
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
// Compile: fxc /T ps_3_0 /E main /Fh GradingPs.h /Vn kGradingPs Grading.ps.hlsl
//
// A 32x32x32 colour cube stored as a 1024x32 strip of 32 slices, indexed r across a slice, g down
// it, b picking the slice. Blue lands between two slices, so those two are read and blended --
// the whole effect is two taps and a lerp, which is why every frame of the modern client can
// afford to go through one.

sampler2D sScene : register(s0);
sampler2D sLut   : register(s1);

// c0: LUT amount, inverse width/height, sharpening strength.
float4 gCtl : register(c0);
// c1: brightness multiplier, contrast about middle grey, inverse gamma.
float4 gFinish : register(c1);
// Forever's 32-entry per-channel curve, evaluated with explicit interpolation.
// Behaviour reference: coa-vfog 4e31ddf shaders/vp_grade.hlsl (GPL-3.0).
sampler2D sCurve : register(s2);
float4 gAuthored : register(c2);
float Curve(float v) {
    float position=saturate(v)*31;
    float lower=floor(position);
    float a=tex2Dlod(sCurve,float4((lower+.5)/32,.5,0,0)).r;
    float b=tex2Dlod(sCurve,float4((min(lower+1,31)+.5)/32,.5,0,0)).r;
    return lerp(a,b,position-lower);
}
float4 main(float2 uv : TEXCOORD0) : COLOR
{
    float4 original=tex2D(sScene,uv);
    float3 c=saturate(original.rgb);
    if (gCtl.w>0)
    {
        float3 left=tex2D(sScene,uv-float2(gCtl.y,0)).rgb;
        float3 right=tex2D(sScene,uv+float2(gCtl.y,0)).rgb;
        float3 up=tex2D(sScene,uv-float2(0,gCtl.z)).rgb;
        float3 down=tex2D(sScene,uv+float2(0,gCtl.z)).rgb;
        float3 low=min(c,min(min(left,right),min(up,down)));
        float3 high=max(c,max(max(left,right),max(up,down)));
        // Local range clamp prevents new light/dark halos at high contrast edges.
        c=clamp(c+gCtl.w*(c-.25*(left+right+up+down)),low,high);
    }
    if (gCtl.x>0)
    {
        float sliceF=c.b*31;
        float slice=floor(sliceF);
        float2 uv0=float2((slice*32+c.r*31+.5)/1024,(c.g*31+.5)/32);
        float2 uv1=float2((min(slice+1,31)*32+c.r*31+.5)/1024,uv0.y);
        float3 graded=lerp(tex2D(sLut,uv0).rgb,tex2D(sLut,uv1).rgb,frac(sliceF));
        c=lerp(c,graded,gCtl.x);
    }
    if(gAuthored.x>0) c=lerp(c,float3(Curve(c.r),Curve(c.g),Curve(c.b)),gAuthored.x);
    c=saturate((c+gFinish.w-.5)*gFinish.y+.5);
    c=saturate(c*gFinish.x);
    c=pow(c,gFinish.z);
    return float4(c,original.a);
}
